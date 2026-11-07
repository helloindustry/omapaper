#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include "backend.h"

#include <QDate>
#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPageSize>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSettings>
#include <QTextBlock>
#include <QUrl>

#include "outline.h"

// The size Omarchy gives its terminal (foot, the default), so TaskPaper's text matches the text around it.
static double omarchyFontSize() {
    QFile file(QDir::homePath() + "/.config/foot/foot.ini");
    if (file.open(QIODevice::ReadOnly)) {
        static const QRegularExpression size(R"(^font\s*=.*:size=(\d+(?:\.\d+)?))", QRegularExpression::MultilineOption);
        const auto m = size.match(QString::fromUtf8(file.readAll()));
        if (m.hasMatch())
            return m.captured(1).toDouble();
    }
    return 9;
}

Backend::Backend(Theme *theme, const QString &initialFile, QObject *parent)
    : QObject(parent), m_theme(theme), m_initialFile(initialFile) {
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, &Backend::refresh);
    // Like TaskPaper, there is no Save command to forget: edits reach the file shortly after typing.
    m_save.setSingleShot(true);
    m_save.setInterval(800);
    connect(&m_save, &QTimer::timeout, this, &Backend::save);
    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(3000);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] { m_notice.clear(); emit noticeChanged(); });
    // Another computer editing the same file (through a sync service) shows up here as a change on disk.
    m_external.setSingleShot(true);
    m_external.setInterval(300); // lets a sync finish writing before we read
    connect(&m_external, &QTimer::timeout, this, &Backend::externalChange);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_external.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_external.start(); });
    connect(m_theme, &Theme::changed, this, &Backend::applyColors);
    m_sidebarWidth = QSettings("taskpaper", "taskpaper").value("sidebarWidth", 240.0).toDouble();
    m_defaultFontSize = omarchyFontSize();
    m_fontDelta = QSettings("taskpaper", "taskpaper").value("fontDelta", 0.0).toDouble();
    m_fontSize = m_defaultFontSize + m_fontDelta;
}

// The font Omarchy has picked for monospace (iA Writer Mono S by default); fontconfig is where Omarchy sets it.
QString Backend::fontFamily() const {
    static const QString family = [] {
        QProcess fc;
        fc.start("fc-match", {"-f", "%{family}", "monospace"});
        if (fc.waitForFinished(500) && fc.exitCode() == 0) {
            const QString name = QString::fromUtf8(fc.readAllStandardOutput()).section(',', 0, 0).trimmed();
            if (!name.isEmpty())
                return name;
        }
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    return family;
}

// Ctrl+= and Ctrl+- change the size by a point; 0 goes back to Omarchy's size. Only the change is remembered,
// so a later change to Omarchy's own font size carries over.
void Backend::changeFontSize(double delta) {
    const double next = delta == 0 ? 0.0 : qBound(6.0 - m_defaultFontSize, m_fontDelta + delta, 32.0 - m_defaultFontSize);
    if (next == m_fontDelta)
        return;
    m_fontDelta = next;
    m_fontSize = m_defaultFontSize + m_fontDelta;
    QSettings("taskpaper", "taskpaper").setValue("fontDelta", m_fontDelta);
    emit fontSizeChanged();
}
QString Backend::title() const { return m_path.isEmpty() ? tr("Untitled") : QFileInfo(m_path).fileName(); }

void Backend::applyColors() {
    if (m_highlighter)
        m_highlighter->setColors(m_theme->accent(), m_theme->muted());
}

void Backend::showNotice(const QString &message, int milliseconds) {
    m_notice = message;
    emit noticeChanged();
    m_noticeTimer.start(milliseconds);
}

void Backend::setError(const QString &message) {
    m_error = message;
    emit errorChanged();
}

void Backend::attach(QQuickTextDocument *document) {
    m_doc = document->textDocument();
    m_highlighter = new Highlighter(m_doc);
    applyColors();
    connect(m_doc, &QTextDocument::contentsChange, this, &Backend::onChange);

    // No file given: open the last one, or start the default list, so typing can begin at once.
    QSettings settings("taskpaper", "taskpaper");
    QString path = m_initialFile;
    if (path.isEmpty() && QFileInfo::exists(settings.value("lastFile").toString()))
        path = settings.value("lastFile").toString();
    if (path.isEmpty())
        path = QDir::homePath() + "/Documents/Tasks.taskpaper";
    load(path);
}

void Backend::load(const QString &requested) {
    // Always absolute, so the remembered file still resolves when the app is next started from somewhere else.
    const QString path = QFileInfo(requested).absoluteFilePath();
    QString text;
    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            setError(tr("Can't read %1").arg(path));
            return;
        }
        text = QString::fromUtf8(file.readAll()).replace("\r\n", "\n");
    }
    m_applying = true;
    m_doc->setPlainText(text);
    m_doc->clearUndoRedoStacks();
    m_applying = false;
    m_dirty = false;
    m_path = path;
    m_disk = text;
    watchFile();
    const int remembered = QSettings("taskpaper", "taskpaper")
                               .value("cursor/" + QString::fromLatin1(path.toUtf8().toBase64(QByteArray::Base64UrlEncoding)), 0).toInt();
    m_cursor = qBound(0, remembered, qMax(0, m_doc->characterCount() - 1));
    m_zoomed = false;
    m_folds.clear();
    m_refold.clear();
    const QString foldKey = "folds/" + QString::fromLatin1(path.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    for (const QString &number : QSettings("taskpaper", "taskpaper").value(foldKey).toString().split(',', Qt::SkipEmptyParts)) {
        const int block = number.toInt();
        if (block < m_doc->blockCount() && foldIndex(block) < 0 && hasChildren(block))
            m_folds.append(QTextCursor(m_doc->findBlockByNumber(block)));
    }
    m_collapsedTags.clear();
    for (const QString &name : QSettings("taskpaper", "taskpaper").value("collapsedTags/" + QString::fromLatin1(path.toUtf8().toBase64(QByteArray::Base64UrlEncoding))).toString().split(',', Qt::SkipEmptyParts))
        m_collapsedTags.insert(name);
    m_pin = QTextCursor();
    QSettings("taskpaper", "taskpaper").setValue("lastFile", path);
    setError({});
    emit fileChanged();
    emit zoomChanged();
    refresh();
    // A caret inside a folded item would be on a hidden line; park it on the visible line above.
    QTextBlock at = m_doc->findBlock(m_cursor);
    while (at.isValid() && !at.isVisible() && at.previous().isValid())
        at = at.previous();
    if (at.isValid() && !at.isVisible())
        at = m_doc->begin();
    if (m_cursor > 0)
        emit cursorRequested(at.isVisible() && at.blockNumber() == m_doc->findBlock(m_cursor).blockNumber()
                                 ? m_cursor : at.position() + at.length() - 1);
}

void Backend::openPath(const QString &path) {
    save(); // also remembers the caret of the file being left
    load(path);
}

// Written beside the file and renamed, so a crash never leaves half a document.
// Prints what you see, so a zoom, search, or fold decides what ends up on paper. The page is made as a PDF
// and handed to CUPS, which is what Omarchy's printer setup configures.
bool Backend::renderPdf(const QString &path) const {
    if (!m_doc)
        return false;
    QString html = "<body style=\"color:black\">";
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next()) {
        if (!b.isVisible())
            continue;
        const QString text = b.text();
        const Item item = parseLine(text);
        if (item.type == Item::Blank) {
            html += "<div>&nbsp;</div>";
            continue;
        }
        QString line = QString("&nbsp;").repeated(item.indent / 2); // margin-left is ignored by Qt's rich text here
        int pos = item.bodyStart;
        for (const auto &tag : item.tags) {
            line += text.mid(pos, tag.start - pos).toHtmlEscaped();
            line += "<span style=\"color:#666666\">" + text.mid(tag.start, tag.length).toHtmlEscaped() + "</span>";
            pos = tag.start + tag.length;
        }
        line += text.mid(pos).toHtmlEscaped();
        QString style;
        if (item.type == Item::Project)
            style = "font-weight:bold;";
        else if (item.type == Item::Note)
            style = "color:#555555;";
        if (item.hasTag("done"))
            style += "text-decoration:line-through;color:#777777;";
        html += QString("<div style=\"white-space:pre-wrap;%1\">%2</div>").arg(style, line);
    }
    html += "</body>";

    QTextDocument page;
    page.setDefaultFont(QFont(fontFamily(), 10));
    page.setDocumentMargin(0);
    page.setHtml(html);
    QPdfWriter pdf(path);
    pdf.setTitle(title());
    pdf.setPageSize(QPageSize(QLocale::system().measurementSystem() == QLocale::ImperialUSSystem ? QPageSize::Letter : QPageSize::A4));
    pdf.setPageMargins(QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);
    page.print(&pdf);
    return QFileInfo(path).size() > 0;
}

void Backend::print() {
    QTemporaryFile file(QDir::tempPath() + "/taskpaper-XXXXXX.pdf");
    file.setAutoRemove(false);
    if (!file.open() || (file.close(), !renderPdf(file.fileName()))) {
        setError(tr("Can't prepare the page for printing"));
        return;
    }
    const QString path = file.fileName();
    auto *lp = new QProcess(this);
    connect(lp, &QProcess::finished, this, [this, lp, path](int code, QProcess::ExitStatus) {
        if (code == 0) {
            showNotice(tr("Sent to the printer"));
        } else {
            // lp says why: no printer set up, printer offline, and so on.
            setError(tr("Can't print: %1").arg(QString::fromUtf8(lp->readAllStandardError()).trimmed()));
        }
        QFile::remove(path);
        lp->deleteLater();
    });
    connect(lp, &QProcess::errorOccurred, this, [this, lp, path](QProcess::ProcessError) {
        setError(tr("Can't print: the lp command is missing"));
        QFile::remove(path);
        lp->deleteLater();
    });
    lp->start("lp", {"-t", title(), path});
}

QVariantList Backend::lineRange(int position) const {
    const QTextBlock b = m_doc->findBlock(position);
    return {b.position(), b.position() + b.length() - 1}; // the line's text, up to but not into the next line
}

bool Backend::caretThemed() const { return QSettings("taskpaper", "taskpaper").value("caretThemed", true).toBool(); }
void Backend::setCaretThemed(bool themed) { QSettings("taskpaper", "taskpaper").setValue("caretThemed", themed); }

void Backend::saveSidebarWidth() { QSettings("taskpaper", "taskpaper").setValue("sidebarWidth", m_sidebarWidth); }

bool Backend::sidebarShown() const { return QSettings("taskpaper", "taskpaper").value("sidebarShown", true).toBool(); }
void Backend::setSidebarShown(bool shown) { QSettings("taskpaper", "taskpaper").setValue("sidebarShown", shown); }

void Backend::toggleTagFold(const QString &name) {
    if (!m_collapsedTags.remove(name))
        m_collapsedTags.insert(name);
    refresh();
    persistCursor();
}

// ---- several carets at once -------------------------------------------------------

QVariantList Backend::extraCursors() const {
    QVariantList positions;
    for (const auto &c : m_extras)
        positions << c.position();
    return positions;
}

void Backend::addCursor(int position, int primary) {
    if (!m_doc)
        return;
    QTextCursor c(m_doc);
    c.setPosition(qBound(0, position, m_doc->characterCount() - 1));
    m_extras.append(c);
    QList<QTextCursor> all = allCursors(primary);
    finishMulti(all);
}

void Backend::clearCursors() {
    if (m_extras.isEmpty())
        return;
    m_extras.clear();
    emit extraCursorsChanged();
}

QList<QTextCursor> Backend::allCursors(int primary) const {
    QList<QTextCursor> all = m_extras;
    QTextCursor p(m_doc);
    p.setPosition(qBound(0, primary, m_doc->characterCount() - 1));
    all.append(p); // the editor's own caret is always last
    return all;
}

// Drops carets that landed on the same spot, keeps the rest as the extra carets, and returns the primary's position.
int Backend::finishMulti(QList<QTextCursor> &all) {
    const int primary = all.last().position();
    QList<QTextCursor> keep;
    QSet<int> seen{primary};
    for (int i = 0; i < all.size() - 1; ++i)
        if (!seen.contains(all[i].position())) {
            seen.insert(all[i].position());
            keep << all[i];
        }
    m_extras = keep;
    emit extraCursorsChanged();
    return primary;
}

int Backend::multiType(int primary, const QString &text) {
    QList<QTextCursor> all = allCursors(primary);
    all.first().beginEditBlock();
    for (auto &c : all)
        c.insertText(text);
    all.first().endEditBlock();
    return finishMulti(all);
}

int Backend::multiKey(int primary, int key) {
    QList<QTextCursor> all = allCursors(primary);
    all.first().beginEditBlock();
    for (auto &c : all) {
        switch (key) {
        case Qt::Key_Backspace: c.deletePreviousChar(); break;
        case Qt::Key_Delete: c.deleteChar(); break;
        case Qt::Key_Left: c.movePosition(QTextCursor::PreviousCharacter); break;
        case Qt::Key_Right: c.movePosition(QTextCursor::NextCharacter); break;
        case Qt::Key_Up: c.movePosition(QTextCursor::Up); break;
        case Qt::Key_Down: c.movePosition(QTextCursor::Down); break;
        case Qt::Key_Home: c.movePosition(QTextCursor::StartOfBlock); break;
        case Qt::Key_End: c.movePosition(QTextCursor::EndOfBlock); break;
        }
    }
    all.first().endEditBlock();
    return finishMulti(all);
}

void Backend::rememberCursor(int position) { m_cursor = position; }

// The caret position is kept per file, so reopening puts you back where you stopped.
void Backend::persistCursor() {
    if (m_path.isEmpty() || !m_doc)
        return;
    QSettings settings("taskpaper", "taskpaper");
    const QString key = QString::fromLatin1(m_path.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    settings.setValue("cursor/" + key, m_cursor);
    // Which items are folded, as line numbers, so the outline reopens the way it was left.
    QStringList folded;
    for (const auto &fold : m_folds + m_refold)
        folded << QString::number(fold.blockNumber());
    settings.setValue("folds/" + key, folded.join(','));
    QStringList tags = m_collapsedTags.values();
    tags.sort();
    settings.setValue("collapsedTags/" + key, tags.join(','));
}

void Backend::save() {
    persistCursor();
    if (!m_doc || !m_dirty || m_path.isEmpty())
        return;
    const QString text = m_doc->toPlainText();
    if (text.isEmpty() && !QFileInfo::exists(m_path))
        return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        setError(tr("Can't save %1").arg(m_path));
        return;
    }
    m_dirty = false;
    setError({});
    showNotice(tr("Saved"), 1500);
}

void Backend::saveNow() {
    save();
    if (!m_path.isEmpty() && m_error.isEmpty())
        showNotice(tr("Saved"), 1500);
}

// The open dialog is the desktop's own, reached through xdg-desktop-portal.
void Backend::openDialog() {
    auto bus = QDBusConnection::sessionBus();
    const QString token = "taskpaper" + QString::number(QRandomGenerator::global()->generate());
    const QString path = "/org/freedesktop/portal/desktop/request/" + bus.baseService().mid(1).replace('.', '_') + "/" + token;
    bus.connect({}, path, "org.freedesktop.portal.Request", "Response", this, SLOT(portalResponse(uint, QVariantMap)));
    auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                               "org.freedesktop.portal.FileChooser", "OpenFile");
    call << QString() << tr("Open") << QVariantMap{{"handle_token", token}};
    bus.asyncCall(call);
}

// Save As is the desktop's own dialog too; the document then continues in the new file.
// A save or a sync replaces the file, which drops it from the watch, so watch its folder too and re-arm every time.
void Backend::watchFile() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    if (m_path.isEmpty())
        return;
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
    m_watcher.addPath(QFileInfo(m_path).absolutePath());
}

// The part of two texts that differs: how much they share at the start and end.
struct Span { int start, oldLength, newLength; };
static Span differ(const QString &a, const QString &b) {
    int p = 0;
    const int most = qMin(a.size(), b.size());
    while (p < most && a[p] == b[p])
        ++p;
    int s = 0;
    while (s < most - p && a[a.size() - 1 - s] == b[b.size() - 1 - s])
        ++s;
    return {p, int(a.size()) - p - s, int(b.size()) - p - s};
}

// The file changed on disk. If it is only someone else's edit, apply it in place so the caret, folds and zoom
// stay put; if we also have unsaved edits, merge when they touch different places, and otherwise keep ours
// and set theirs aside as a copy so nothing is lost.
void Backend::externalChange() {
    watchFile();
    if (!m_doc || m_path.isEmpty())
        return;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return; // being replaced; the folder watch fires again when it is back
    const QString remote = QString::fromUtf8(file.readAll()).replace("\r\n", "\n");
    file.close();
    if (remote == m_disk)
        return;
    const QString local = m_doc->toPlainText();
    if (local == remote) {
        m_disk = remote;
        m_dirty = false;
        return;
    }

    const QString base = m_disk;
    const Span theirs = differ(base, remote);
    const Span ours = differ(base, local);
    int at, removeLength;
    QString insert;
    bool merged = false;
    if (local == base) {
        at = theirs.start;
        removeLength = theirs.oldLength;
        insert = remote.mid(theirs.start, theirs.newLength);
    } else if (ours.start + ours.oldLength < theirs.start || theirs.start + theirs.oldLength < ours.start) {
        // Different places: apply their change, shifted by however much ours moved the text before it.
        const int shift = ours.start < theirs.start ? ours.newLength - ours.oldLength : 0;
        at = theirs.start + shift;
        removeLength = theirs.oldLength;
        insert = remote.mid(theirs.start, theirs.newLength);
        merged = true;
    } else {
        const QFileInfo info(m_path);
        const QString copy = info.absolutePath() + "/" + info.completeBaseName() + " (conflict "
                             + QDateTime::currentDateTime().toString("yyyy-MM-dd HHmm") + ")."
                             + (info.suffix().isEmpty() ? "taskpaper" : info.suffix());
        QSaveFile out(copy);
        if (out.open(QIODevice::WriteOnly) && out.write(remote.toUtf8()) >= 0 && out.commit())
            showNotice(tr("Edited elsewhere in the same place. Kept yours; theirs is saved as %1").arg(QFileInfo(copy).fileName()));
        else
            setError(tr("Edited elsewhere in the same place, and their copy couldn't be saved"));
        m_disk = remote;
        m_dirty = true;
        m_save.start();
        return;
    }

    m_applying = true; // not a user edit: it must not pin a line, mark the file changed, or save
    QTextCursor c(m_doc);
    c.setPosition(at);
    c.setPosition(at + removeLength, QTextCursor::KeepAnchor);
    c.insertText(insert);
    m_applying = false;
    m_disk = remote;
    m_dirty = merged;
    if (merged)
        m_save.start();
    m_refresh.start();
    showNotice(tr("Updated from another computer"));
}

void Backend::saveAsDialog() {
    auto bus = QDBusConnection::sessionBus();
    const QString token = "taskpaper" + QString::number(QRandomGenerator::global()->generate());
    const QString path = "/org/freedesktop/portal/desktop/request/" + bus.baseService().mid(1).replace('.', '_') + "/" + token;
    bus.connect({}, path, "org.freedesktop.portal.Request", "Response", this, SLOT(saveAsResponse(uint, QVariantMap)));
    auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                               "org.freedesktop.portal.FileChooser", "SaveFile");
    call << QString() << tr("Save As") << QVariantMap{{"handle_token", token}, {"current_name", title().endsWith(".taskpaper") || title().endsWith(".txt") ? title() : "Tasks.taskpaper"}};
    bus.asyncCall(call);
}

void Backend::saveAsResponse(uint code, const QVariantMap &results) {
    if (code != 0 || !m_doc)
        return;
    QVariant uris = results.value("uris");
    QStringList list = uris.canConvert<QDBusArgument>() ? qdbus_cast<QStringList>(uris.value<QDBusArgument>())
                                                        : uris.toStringList();
    if (list.isEmpty())
        return;
    save(); // finish the old file first
    m_path = QUrl(list.first()).toLocalFile();
    QSettings("taskpaper", "taskpaper").setValue("lastFile", m_path);
    QFile(m_path).open(QIODevice::WriteOnly | QIODevice::Append); // creates it, so an empty document still gets a file
    m_disk.clear();
    m_dirty = true;
    save();
    emit fileChanged();
}

void Backend::portalResponse(uint code, const QVariantMap &results) {
    if (code != 0)
        return;
    QVariant uris = results.value("uris");
    QStringList list = uris.canConvert<QDBusArgument>() ? qdbus_cast<QStringList>(uris.value<QDBusArgument>())
                                                        : uris.toStringList();
    if (!list.isEmpty())
        openPath(QUrl(list.first()).toLocalFile());
}

void Backend::onChange(int position, int removed, int added) {
    if (m_applying)
        return;
    m_dirty = true;
    // One typed character: maybe it completes a tag the document already uses.
    if (removed == 0 && added == 1 && m_extras.isEmpty())
        QTimer::singleShot(0, this, [this, position] { suggestTag(position); });
    // The line being typed stays visible even if it doesn't match the search yet.
    m_pin = QTextCursor(m_doc->findBlock(position));
    m_refresh.start();
    m_save.start();
}

// ---- filtering, zooming, sidebar -------------------------------------------------

// "@M" offers "@MIT" when a tag starting with M exists. The rest is added as selected text, so typing on
// replaces it and Tab or Right accepts it. Nothing is offered after "@" alone, nor after a deletion.
void Backend::suggestTag(int position) {
    if (!m_doc || m_applying)
        return;
    const QTextBlock b = m_doc->findBlock(position);
    const QString text = b.text();
    const int col = position - b.position() + 1; // just after the typed character
    if (col > text.size() || (col < text.size() && text[col].isLetterOrNumber()))
        return; // only at the end of a word
    static const QRegularExpression partial(R"((?<!\S)@([\w.\-]+)$)");
    const auto m = partial.match(text.left(col));
    if (!m.hasMatch())
        return;
    const QString prefix = m.captured(1);
    QString best;
    for (auto it = m_tagCounts.begin(); it != m_tagCounts.end(); ++it)
        if (it.key().size() > prefix.size() && it.key().startsWith(prefix) // same case, so what you typed stays as typed
            && (best.isEmpty() || it.value() > m_tagCounts.value(best)))
            best = it.key();
    if (best.isEmpty())
        return;
    const QString rest = best.mid(prefix.size());
    QTextCursor c(m_doc);
    c.setPosition(position + 1);
    m_applying = true; // not a user edit: no autosave churn, no second suggestion
    c.insertText(rest);
    m_applying = false;
    m_dirty = true;
    m_save.start();
    m_refresh.start();
    emit suggestion(position + 1, position + 1 + rest.size());
}

void Backend::setQuery(const QString &query) {
    if (query == m_query)
        return;
    m_query = query;
    m_pin = QTextCursor();
    emit queryChanged();
    refresh();
}

void Backend::refresh() {
    if (!m_doc)
        return;
    QVector<Item> items;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next())
        items.append(parseLine(b.text()));
    const int n = items.size();

    // Parent and end of each item's subtree; blank lines belong to whatever surrounds them.
    QVector<int> parent(n, -1), end(n, n), stack;
    for (int i = 0; i < n; ++i) {
        if (items[i].type == Item::Blank)
            continue;
        while (!stack.isEmpty() && endsSubtree(items[stack.last()], items[i]))
            end[stack.takeLast()] = i;
        parent[i] = stack.isEmpty() ? -1 : stack.last();
        stack.append(i);
    }

    // The Searches project holds saved searches; it is not a project or a result.
    QVector<bool> inSearches(n, false);
    for (int i = 0; i < n; ++i)
        if (items[i].type == Item::Project && items[i].indent == 0 && items[i].title == "Searches")
            for (int j = i; j < end[i]; ++j)
                inSearches[j] = true;

    QVector<bool> visible(n, true);
    const Query query = Query::parse(m_query);
    if (m_highlighter)
        m_highlighter->setSearchTerms(query.textTerms());
    if (!query.isEmpty()) {
        visible.fill(false);
        for (int i = 0; i < n; ++i) {
            if (items[i].type == Item::Blank || inSearches[i] || !query.matches(items[i]))
                continue;
            for (int p = i; p >= 0; p = parent[p])
                visible[p] = true;
            for (int j = i; j < end[i]; ++j)
                visible[j] = true;
        }
    }
    if (!query.isEmpty())
        for (int i = 0; i < n; ++i)
            if (items[i].type != Item::Blank && query.excludes(items[i]))
                for (int j = i; j < end[i]; ++j)
                    visible[j] = false;
    if (m_zoomed) {
        const int root = m_zoom.blockNumber();
        for (int i = 0; i < n; ++i)
            visible[i] = visible[i] && i >= root && i < end[root];
        if (root < n)
            visible[root] = true;
    }
    // A fold hides an item's children. Folds whose item lost its children are dropped.
    // The pinned line (the one just typed on) stays visible against a search, but never against a fold.
    if (!m_pin.isNull() && m_pin.blockNumber() < n)
        visible[m_pin.blockNumber()] = true;

    // Saved searches show only in the sidebar, never in the editor.
    // The empty line at the very end stays, so the caret never ends up inside the hidden part.
    int lastContent = n - 1;
    while (lastContent >= 0 && items[lastContent].type == Item::Blank)
        --lastContent;
    for (int i = 0; i < n; ++i)
        if (inSearches[i] && i <= lastContent)
            visible[i] = false;

    // Folding rule: the lines directly beneath an item that are indented deeper belong to it and fold with it.
    // The first line that is not indented deeper, empty lines included, ends them.
    auto foldRegionEnd = [&](int i) {
        int j = i + 1;
        while (j < n && items[j].indent > items[i].indent)
            ++j;
        return j;
    };
    auto parent_has_children = [&](int i) {
        if (items[i].type == Item::Blank)
            return false;
        for (int j = i + 1; j < foldRegionEnd(i); ++j)
            if (items[j].type != Item::Blank)
                return true;
        return false;
    };
    for (int f = m_folds.size() - 1; f >= 0; --f) {
        const int b = m_folds[f].blockNumber();
        if (b >= n || !parent_has_children(b)) {
            m_folds.removeAt(f);
            continue;
        }
        for (int j = b + 1; j < foldRegionEnd(b); ++j)
            visible[j] = false;
    }

    m_applying = true;
    int i = 0, from = -1;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next(), ++i) {
        if (b.isVisible() != visible[i]) {
            b.setVisible(visible[i]);
            from = b.position();
        }
    }
    // Hiding a block doesn't relayout by itself; mark the whole document dirty; a partial range leaves middle blocks unrendered.
    if (from >= 0)
        m_doc->markContentsDirty(0, m_doc->characterCount());
    m_applying = false;
    // The editor only redraws lines it is told changed, and a highlighter pass is what tells it.
    if (from >= 0 && m_highlighter) {
        m_applying = true;
        m_highlighter->rehighlight();
        m_applying = false;
    }

    QVariantList marks;
    for (int i = 0; i < n; ++i)
        if (visible[i] && parent_has_children(i))
            marks << QVariantMap{{"block", i}, {"pos", m_doc->findBlockByNumber(i).position()},
                                 {"folded", foldIndex(i) >= 0}};
    if (marks != m_foldMarks) {
        m_foldMarks = marks;
        emit foldsChanged();
    }

    QVariantList projects, tags, searches;
    QMap<QString, QStringList> values;
    QMap<QString, int> counts;
    for (int i = 0; i < n; ++i) {
        const Item &item = items[i];
        if (item.hasTag("search")) {
            if (!item.tagValue("search").isEmpty())
                searches << QVariantMap{{"kind", "search"}, {"label", item.title},
                                        {"query", item.tagValue("search")}, {"block", i}};
            continue;
        }
        if (inSearches[i])
            continue;
        if (item.type == Item::Project)
            projects << QVariantMap{{"kind", "project"}, {"label", item.title}, {"level", item.indent / 4}, {"block", i}};
        for (const auto &tag : item.tags) {
            counts[tag.name]++;
            QStringList &list = values[tag.name];
            if (tag.hasValue && !tag.value.isEmpty() && !list.contains(tag.value))
                list << tag.value;
        }
    }
    m_tagCounts = counts;
    // Each tag, and under it the distinct values it has been given; a tag with values can be collapsed.
    for (auto it = values.begin(); it != values.end(); ++it) {
        QStringList sorted = it.value();
        sorted.sort(Qt::CaseInsensitive);
        const bool collapsed = m_collapsedTags.contains(it.key());
        tags << QVariantMap{{"kind", "tag"}, {"label", "@" + it.key()}, {"name", it.key()}, {"query", "@" + it.key()},
                            {"level", 0}, {"foldable", !sorted.isEmpty()}, {"collapsed", collapsed}};
        if (collapsed)
            continue;
        for (const auto &value : sorted)
            tags << QVariantMap{{"kind", "tag"}, {"label", value}, {"level", 1}, {"foldable", false},
                                {"query", "@" + it.key() + " = " + value}};
    }

    QVariantList sidebar;
    sidebar << QVariantMap{{"kind", "all"}, {"label", tr("Home")}};
    auto section = [&](const QString &name, const QVariantList &rows) {
        if (!rows.isEmpty())
            sidebar << QVariantMap{{"kind", "header"}, {"label", name}} << rows;
    };
    section(tr("Projects"), projects);
    section(tr("Tags"), tags);
    section(tr("Searches"), searches);
    if (sidebar != m_sidebar) {
        m_sidebar = sidebar;
        emit sidebarChanged();
    }
    emit zoomChanged(); // the zoomed line may have been renamed
    if (!m_extras.isEmpty())
        emit extraCursorsChanged();
}

int Backend::parentBlock(int block) const {
    const Item item = parseLine(m_doc->findBlockByNumber(block).text());
    for (QTextBlock b = m_doc->findBlockByNumber(block).previous(); b.isValid(); b = b.previous()) {
        const Item other = parseLine(b.text());
        if (other.type != Item::Blank && !endsSubtree(other, item))
            return b.blockNumber();
    }
    return -1;
}

QString Backend::zoomPath() const {
    if (!m_zoomed || !m_doc)
        return {};
    QStringList path;
    for (int b = m_zoom.blockNumber(); b >= 0; b = parentBlock(b))
        path.prepend(parseLine(m_doc->findBlockByNumber(b).text()).title);
    return path.join("  ›  ");
}

void Backend::search(const QString &query) {
    m_zoomed = false;
    releaseRefolds();
    m_query = query;
    m_pin = QTextCursor();
    emit queryChanged();
    refresh();
    // The caret may be on a line that just disappeared; put it on the first one still showing.
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next())
        if (b.isVisible()) {
            emit cursorRequested(b.position() + b.length() - 1);
            break;
        }
}

// The search is a chain of terms joined by "and". Clicking builds it up one term at a time, and clicking a term
// that is already there takes it out again.
static QStringList searchParts(const QString &query) {
    if (query.trimmed().isEmpty())
        return {};
    static const QRegularExpression andSplit(R"(\s+and\s+)", QRegularExpression::CaseInsensitiveOption);
    QStringList parts = query.trimmed().split(andSplit, Qt::SkipEmptyParts);
    for (auto &part : parts)
        part = part.trimmed();
    return parts;
}

int Backend::termState(const QString &term) const {
    for (const QString &part : searchParts(m_query)) {
        if (part.compare(term, Qt::CaseInsensitive) == 0)
            return 1;
        if (part.compare("not " + term, Qt::CaseInsensitive) == 0)
            return 2;
    }
    return 0;
}

static bool hasOr(const QString &query) {
    return query.contains(QRegularExpression(R"(\s+or\s+)", QRegularExpression::CaseInsensitiveOption));
}

// `wanted` is the clause to end up with ("@x" or "not @x"); `opposite` is the clause it replaces.
static QString toggledQuery(const QString &query, const QString &wanted, const QString &opposite) {
    QStringList parts = searchParts(query);
    if (!parts.isEmpty() && hasOr(query))
        parts.clear(); // an "or" search can't take an extra "and" safely, so start fresh
    for (int i = parts.size() - 1; i >= 0; --i)
        if (parts[i].compare(wanted, Qt::CaseInsensitive) == 0) {
            parts.removeAt(i);
            return parts.join(" and ");
        }
    for (auto &part : parts)
        if (part.compare(opposite, Qt::CaseInsensitive) == 0) {
            part = wanted;
            return parts.join(" and ");
        }
    parts << wanted;
    return parts.join(" and ");
}

void Backend::include(const QString &term) { search(toggledQuery(m_query, term, "not " + term)); }
void Backend::exclude(const QString &term) { search(toggledQuery(m_query, "not " + term, term)); }

void Backend::showProject(int block) {
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    zoomToBlock(block);
}

void Backend::showAll() { search({}); }

int Backend::foldEnd(int block) const {
    const int indent = parseLine(m_doc->findBlockByNumber(block).text()).indent;
    int i = block + 1;
    for (QTextBlock b = m_doc->findBlockByNumber(i); b.isValid() && parseLine(b.text()).indent > indent; b = b.next())
        ++i;
    return i;
}

int Backend::enclosingParent(int block) const {
    const int indent = parseLine(m_doc->findBlockByNumber(block).text()).indent;
    for (QTextBlock b = m_doc->findBlockByNumber(block).previous(); b.isValid(); b = b.previous()) {
        const Item item = parseLine(b.text());
        if (item.type != Item::Blank && item.indent < indent)
            return b.blockNumber();
    }
    return -1;
}

bool Backend::hasChildren(int block) const {
    const QTextBlock start = m_doc->findBlockByNumber(block);
    if (parseLine(start.text()).type == Item::Blank)
        return false;
    for (int j = block + 1; j < foldEnd(block); ++j)
        if (parseLine(m_doc->findBlockByNumber(j).text()).type != Item::Blank)
            return true;
    return false;
}

int Backend::foldIndex(int block) const {
    for (int i = 0; i < m_folds.size(); ++i)
        if (m_folds[i].blockNumber() == block)
            return i;
    return -1;
}

void Backend::toggleFoldBlock(int block) {
    if (!m_doc || !m_doc->findBlockByNumber(block).isValid())
        return;
    const int at = foldIndex(block);
    if (at >= 0)
        m_folds.removeAt(at);
    else if (hasChildren(block))
        m_folds.append(QTextCursor(m_doc->findBlockByNumber(block)));
    else
        return;
    // Keep the caret on the folded line instead of inside the lines that just vanished.
    emit cursorRequested(m_doc->findBlockByNumber(block).position() + m_doc->findBlockByNumber(block).length() - 1);
    refresh();
    persistCursor();
}

// On a line with nothing beneath it, the fold keys fold the line's parent instead and leave the caret on it.
void Backend::toggleFold(int position) {
    int block = m_doc->findBlock(position).blockNumber();
    if (!hasChildren(block)) {
        block = enclosingParent(block);
        if (block < 0)
            return;
    }
    toggleFoldBlock(block);
}

// Markdown emphasis on the selection, in one edit so one Undo takes it back. A marker only counts as a whole
// run, so the "*" of "**bold**" is never taken for italic. With nothing selected it works on the span around
// the caret: at its end the caret steps past the closing marker (and back in on a second press), anywhere
// else inside it the markers come off; outside any span it opens an empty pair with the caret between.
// With nothing selected, the word touching the caret becomes the selection. Returns false when no word touches it.
static bool selectTouchingWord(const QString &all, int &start, int &end) {
    if (start != end)
        return true;
    auto isWord = [&](int i) { return i >= 0 && i < all.size() && (all[i].isLetterOrNumber() || all[i] == '_'); };
    if (!isWord(start - 1) && !isWord(start))
        return false;
    int a = start, b = start;
    while (isWord(a - 1)) --a;
    while (isWord(b)) ++b;
    start = a;
    end = b;
    return true;
}

void Backend::toggleWrap(int start, int end, const QString &marker) {
    if (!m_doc || marker.isEmpty())
        return;
    const QString all = m_doc->toPlainText();
    const int size = all.size(), m = marker.size();
    const QChar mark = marker[0];
    start = qBound(0, start, size);
    end = qBound(start, end, size);
    selectTouchingWord(all, start, end);
    auto run = [&](int pos, int step, int limit) {
        int r = 0;
        while (pos + r * step != limit && pos + r * step >= 0 && pos + r * step < size && all[pos + r * step] == mark)
            ++r;
        return r;
    };
    auto holds = [&](int r) { return r == m || (mark == '*' && r == 3); };
    auto hasMarker = [&](const QString &part) {
        for (int i = 0; i < part.size();) {
            if (part[i] != mark) { ++i; continue; }
            int j = i;
            while (j < part.size() && part[j] == mark) ++j;
            if (holds(j - i)) return true;
            i = j;
        }
        return false;
    };
    auto replace = [&](int a, int b, const QString &with, int selStart, int selEnd) {
        QTextCursor c(m_doc);
        c.setPosition(a);
        c.setPosition(b, QTextCursor::KeepAnchor);
        c.insertText(with);
        emit moved(a + selStart, a + selEnd);
    };

    if (start == end) {
        // Look for a span of this marker on the caret's line.
        const QTextBlock block = m_doc->findBlock(start);
        const QString line = block.text();
        const int base = block.position();
        auto isMark = [&](int i) {
            if (i < 0 || i + m > line.size() || line.mid(i, m) != marker)
                return false;
            int before = 0, after = 0;
            for (int k = i - 1; k >= 0 && line[k] == mark; --k) ++before;
            for (int k = i + m; k < line.size() && line[k] == mark; ++k) ++after;
            return holds(before + m + after) && before == 0;
        };
        auto word = [&](int i) { return i >= 0 && i < line.size() && line[i].isLetterOrNumber(); };
        for (int i = 0; i < line.size(); ++i) {
            if (!isMark(i) || i + m >= line.size() || line[i + m].isSpace() || word(i - 1))
                continue;
            int j = i + m + 1;
            for (; j < line.size(); ++j)
                if (isMark(j) && !line[j - 1].isSpace() && !word(j + m))
                    break;
            if (j >= line.size())
                continue;
            const int caret = start - base;
            if (caret >= i && caret <= j + m) {
                if (caret == j)
                    emit moved(base + j + m, base + j + m);
                else if (caret == j + m)
                    emit moved(base + j, base + j);
                else {
                    const QString inner = line.mid(i + m, j - i - m);
                    replace(base + i, base + j + m, inner, caret - i > m ? qMin(caret - i - m, int(inner.size())) : 0,
                            caret - i > m ? qMin(caret - i - m, int(inner.size())) : 0);
                }
                return;
            }
            i = j + m - 1;
        }
        replace(start, start, marker + marker, m, m);
        return;
    }

    const QString selected = all.mid(start, end - start);
    const QString inner = selected.mid(m, selected.size() - 2 * m);
    if (holds(run(start - 1, -1, -1)) && holds(run(end, 1, size)) && !hasMarker(selected)) {
        replace(start - m, end + m, selected, 0, selected.size());
    } else if (selected.size() >= 2 * m && holds(run(start, 1, end)) && holds(run(end - 1, -1, start - 1))
               && !hasMarker(inner) && (start == 0 || all[start - 1] != mark) && (end >= size || all[end] != mark)) {
        replace(start, end, inner, 0, inner.size());
    } else {
        replace(start, end, marker + selected + marker, m, m + selected.size());
    }
}

// [label](url): the selection becomes the label, and a URL on the clipboard becomes the target.
void Backend::insertLink(int start, int end) {
    if (!m_doc)
        return;
    const int size = m_doc->characterCount() - 1;
    start = qBound(0, start, size);
    end = qBound(start, end, size);
    selectTouchingWord(m_doc->toPlainText(), start, end);
    QTextCursor c(m_doc);
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);
    const QString selected = c.selectedText().replace(QChar(0x2029), '\n');
    QString url = QGuiApplication::clipboard()->text().trimmed();
    static const QRegularExpression looksLikeUrl("^(https?://|mailto:)\\S+$");
    if (!looksLikeUrl.match(url).hasMatch())
        url.clear();
    const QString label = selected.isEmpty() ? QStringLiteral("link text") : selected;
    const QString markdown = "[" + label + "](" + (url.isEmpty() ? QStringLiteral("https://") : url) + ")";
    c.insertText(markdown);
    if (selected.isEmpty())
        emit moved(start + 1, start + 1 + label.size());
    else if (url.isEmpty())
        emit moved(start + label.size() + 3, start + markdown.size() - 1);
    else
        emit moved(start + markdown.size(), start + markdown.size());
}

// Collapse or expand every item in the selection; "completely" does the same to all their descendants.
void Backend::foldItems(int start, int end, bool collapse, bool completely) {
    if (!m_doc)
        return;
    const auto range = blockRange(start, end);
    for (int i = range.first; i <= range.second; ++i) {
        int base = i;
        if (collapse && !hasChildren(i)) {
            // Nothing beneath this line to fold: fold the line it sits inside instead.
            base = enclosingParent(i);
            if (base < 0)
                continue;
        }
        QList<int> targets{base};
        if (completely)
            for (int j = base + 1; j < foldEnd(base); ++j)
                targets << j;
        for (int t : targets) {
            if (!hasChildren(t))
                continue;
            const int at = foldIndex(t);
            if (collapse && at < 0)
                m_folds.append(QTextCursor(m_doc->findBlockByNumber(t)));
            else if (!collapse && at >= 0)
                m_folds.removeAt(at);
        }
    }
    if (collapse)
        keepCaretVisible(start);
    refresh();
    persistCursor();
}

bool Backend::shiftHeld() const { return QGuiApplication::keyboardModifiers() & Qt::ShiftModifier; }

void Backend::toggleFoldAll() {
    // Expand only when every foldable item is already folded; otherwise fold everything.
    bool allFolded = true, any = false;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next())
        if (hasChildren(b.blockNumber())) {
            any = true;
            if (foldIndex(b.blockNumber()) < 0) { allFolded = false; break; }
        }
    foldAll(!(any && allFolded));
}

// Leave the caret exactly where it is. Only when a fold now hides its line does it move, to the fold's line, same column.
void Backend::keepCaretVisible(int position) {
    if (!m_doc || position < 0)
        return;
    const QTextBlock cb = m_doc->findBlock(position);
    if (!cb.isValid())
        return;
    int hider = -1;
    for (const auto &fold : m_folds) {
        const int fb = fold.blockNumber();
        if (fb < cb.blockNumber() && cb.blockNumber() < foldEnd(fb) && (hider < 0 || fb < hider))
            hider = fb;
    }
    if (hider < 0)
        return;
    const QTextBlock hb = m_doc->findBlockByNumber(hider);
    emit cursorRequested(hb.position() + qMin(position - cb.position(), qMax(hb.length() - 1, 0)));
}

void Backend::foldAll(bool collapse, int position) {
    m_folds.clear();
    if (collapse)
        for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next())
            if (hasChildren(b.blockNumber()))
                m_folds.append(QTextCursor(b));
    keepCaretVisible(position);
    refresh();
    persistCursor();
}

void Backend::foldOthers(int position) {
    const int cur = m_doc->findBlock(position).blockNumber();
    m_folds.clear();
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next()) {
        const int n = b.blockNumber();
        if (!hasChildren(n))
            continue;
        const bool keepsOpen = n == cur || (n < cur && cur < foldEnd(n));
        if (!keepsOpen)
            m_folds.append(QTextCursor(b));
    }
    refresh();
    persistCursor();
}

void Backend::releaseRefolds() {
    for (int i = m_refold.size() - 1; i >= 0; --i) {
        const int b = m_refold[i].blockNumber();
        const bool stillInside = m_zoomed && m_zoom.blockNumber() >= b && m_zoom.blockNumber() < foldEnd(b);
        if (stillInside)
            continue;
        if (foldIndex(b) < 0 && hasChildren(b))
            m_folds.append(QTextCursor(m_doc->findBlockByNumber(b)));
        m_refold.removeAt(i);
    }
}

void Backend::zoomToBlock(int block) {
    if (!m_doc || !m_doc->findBlockByNumber(block).isValid())
        return;
    m_zoom = QTextCursor(m_doc->findBlockByNumber(block));
    m_zoomed = true;
    // Whatever was unfolded for an earlier focus is folded again unless the new focus is still inside it.
    releaseRefolds();
    // Focusing into a folded item shows what is inside it. It is remembered, and folded again on the way out.
    if (const int at = foldIndex(block); at >= 0) {
        m_refold.append(m_folds[at]);
        m_folds.removeAt(at);
    }
    m_pin = QTextCursor();
    refresh();
    const QTextBlock root = m_doc->findBlockByNumber(block);
    emit cursorRequested(root.position() + root.length() - 1);
}

void Backend::zoomIn(int position) {
    const QTextBlock b = m_doc->findBlock(position);
    if (parseLine(b.text()).type != Item::Blank)
        zoomToBlock(b.blockNumber());
}

void Backend::clearZoom() {
    m_zoomed = false;
    releaseRefolds();
    m_pin = QTextCursor();
    refresh();
}

void Backend::zoomOut() {
    if (!m_zoomed)
        return;
    const int parent = parentBlock(m_zoom.blockNumber());
    if (parent < 0)
        clearZoom();
    else
        zoomToBlock(parent);
}

void Backend::saveSearch() {
    if (!m_doc || m_query.trimmed().isEmpty())
        return;
    // Saved searches live in the document itself, under a "Searches:" project, as TaskPaper keeps them.
    int at = -1;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next()) {
        const Item item = parseLine(b.text());
        if (item.type == Item::Project && item.indent == 0 && item.title == "Searches") { at = b.blockNumber(); break; }
    }
    QTextCursor c(m_doc);
    c.beginEditBlock();
    if (at < 0) {
        c.movePosition(QTextCursor::End);
        c.insertText((m_doc->isEmpty() ? "" : "\n") + QString("Searches:"));
    } else {
        c.setPosition(m_doc->findBlockByNumber(at).position());
        c.movePosition(QTextCursor::EndOfBlock);
    }
    QString name = m_query;
    name.remove('@');
    c.insertText("\n\t" + name.trimmed() + " @search(" + m_query.trimmed() + ")");
    c.endEditBlock();
}

void Backend::deleteBlock(int block) {
    QTextBlock b = m_doc->findBlockByNumber(block);
    if (!b.isValid())
        return;
    QTextCursor c(b);
    c.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
    if (c.position() == b.position()) { // the last line has no newline after it
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    }
    c.removeSelectedText();
}

// ---- editing ---------------------------------------------------------------------

QPair<int, int> Backend::blockRange(int start, int end) const {
    const QTextBlock first = m_doc->findBlock(start);
    QTextBlock last = m_doc->findBlock(end);
    if (end > start && last.position() == end && last.previous().isValid())
        last = last.previous(); // a selection ending at a line start doesn't include that line
    return {first.blockNumber(), last.blockNumber()};
}

int Backend::subtreeEnd(int block) const {
    const Item root = parseLine(m_doc->findBlockByNumber(block).text());
    int i = block + 1;
    for (QTextBlock b = m_doc->findBlockByNumber(i); b.isValid(); b = b.next(), ++i) {
        const Item item = parseLine(b.text());
        if (item.type != Item::Blank && endsSubtree(root, item))
            break;
    }
    return i;
}

// Find and replace only touch lines you can see, so a filter or fold never hides a change from you.
int Backend::findNext(const QString &text, int from, bool backwards) {
    if (!m_doc || text.isEmpty())
        return -1;
    const QTextDocument::FindFlags flags = backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags();
    for (int pass = 0; pass < 2; ++pass) { // the second pass wraps around
        int pos = pass == 0 ? from : (backwards ? m_doc->characterCount() - 1 : 0);
        for (;;) {
            const QTextCursor found = m_doc->find(text, pos, flags);
            if (found.isNull())
                break;
            if (m_doc->findBlock(found.selectionStart()).isVisible())
                return found.selectionStart();
            pos = backwards ? found.selectionStart() : found.selectionEnd();
        }
    }
    return -1;
}

int Backend::replaceRange(int start, int end, const QString &with) {
    QTextCursor c(m_doc);
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);
    c.insertText(with);
    return c.position();
}

int Backend::replaceAll(const QString &text, const QString &with) {
    if (!m_doc || text.isEmpty())
        return 0;
    int count = 0, pos = 0;
    QTextCursor edit(m_doc);
    edit.beginEditBlock(); // one undo step
    for (;;) {
        QTextCursor found = m_doc->find(text, pos);
        if (found.isNull())
            break;
        if (m_doc->findBlock(found.selectionStart()).isVisible()) {
            found.insertText(with);
            ++count;
            pos = found.position();
        } else {
            pos = found.selectionEnd();
        }
    }
    edit.endEditBlock();
    return count;
}

// "@due(2026-11-10)" gives "@due = 2026-11-10" and a bare "@home" gives "@home": the same searches the sidebar makes.
QString Backend::tagAt(int position) const {
    if (!m_doc || position < 0)
        return {};
    const QTextBlock b = m_doc->findBlock(position);
    const int column = position - b.position();
    for (const auto &tag : parseLine(b.text()).tags)
        if (column >= tag.start && column <= tag.start + tag.length)
            return "@" + tag.name + (tag.hasValue && !tag.value.isEmpty() ? " = " + tag.value : QString());
    return {};
}

QString Backend::linkAt(int position) const {
    if (!m_doc || position < 0)
        return {};
    const QTextBlock b = m_doc->findBlock(position);
    const int column = position - b.position();
    for (const auto &link : findLinks(b.text()))
        if (column >= link.start && column <= link.start + link.length)
            return link.url;
    return {};
}

void Backend::setHover(int start, int end) {
    if (!m_highlighter)
        return;
    // Restyling a line is not an edit, so it must not mark the document changed or save it.
    const bool applying = m_applying;
    m_applying = true;
    m_highlighter->setHover(start, end);
    m_applying = applying;
}

// Where a tag or link sits in the document, so the editor can tell a click on it from a click in the blank space after it.
QVariantList Backend::tagSpan(int position) const {
    if (!m_doc || position < 0)
        return {};
    const QTextBlock b = m_doc->findBlock(position);
    const int column = position - b.position();
    for (const auto &tag : parseLine(b.text()).tags)
        if (column >= tag.start && column <= tag.start + tag.length)
            return {b.position() + tag.start, b.position() + tag.start + tag.length};
    return {};
}

QVariantList Backend::linkSpan(int position) const {
    if (!m_doc || position < 0)
        return {};
    const QTextBlock b = m_doc->findBlock(position);
    const int column = position - b.position();
    for (const auto &link : findLinks(b.text()))
        if (column >= link.start && column <= link.start + link.length)
            return {b.position() + link.start, b.position() + link.start + link.length};
    return {};
}

// Enter continues a list: a new task under a task, the same indent otherwise.
bool Backend::newline(int position) {
    const QTextBlock b = m_doc->findBlock(position);
    const Item item = parseLine(b.text());
    const int column = position - b.position();
    if (item.type == Item::Blank || column < item.textStart)
        return false;
    const QString lead = b.text().left(item.bodyStart); // the line's own indentation, tabs or spaces
    QTextCursor c(m_doc);
    c.beginEditBlock();
    if (foldIndex(b.blockNumber()) >= 0 && column >= b.text().size()) {
        // Enter at the end of a folded item: it stays folded, and the new line goes after everything hidden
        // inside it, at the item's own level, which is the next line you would see.
        const int last = foldEnd(b.blockNumber()) - 1; // everything hidden inside it ends here
        const QTextBlock tail = m_doc->findBlockByNumber(last);
        c.setPosition(tail.position() + tail.length() - 1);
        c.insertText("\n" + lead + (item.type == Item::Task ? "- " : ""));
        const int caret = c.position();
        c.endEditBlock();
        emit cursorRequested(caret);
        return true;
    }
    c.setPosition(position);
    if (item.type == Item::Task && item.title.isEmpty() && item.tags.isEmpty()) {
        // Enter on an empty task ends the list, and still adds the line the key asked for,
        // so repeated Enters keep making room instead of undoing the dash.
        c.setPosition(b.position() + item.bodyStart);
        c.setPosition(b.position() + b.length() - 1, QTextCursor::KeepAnchor);
        c.insertText("\n" + lead);
    } else if (item.type == Item::Project) {
        // After a project the next thing typed is almost always its first task, one level in.
        c.insertText("\n" + lead + "\t- ");
    } else {
        c.insertText("\n" + lead + (item.type == Item::Task ? "- " : ""));
    }
    c.endEditBlock();
    return true;
}

void Backend::indent(int start, int end, int direction) {
    const auto range = blockRange(start, end);
    QTextCursor c(m_doc);
    c.beginEditBlock();
    for (int i = range.first; i <= range.second; ++i) {
        const QTextBlock b = m_doc->findBlockByNumber(i);
        c.setPosition(b.position());
        if (direction > 0) {
            // A lone empty line can be indented before anything is typed; blank lines inside a selection stay blank.
            if (!b.text().isEmpty() || range.first == range.second)
                c.insertText("\t");
        } else if (b.text().startsWith('\t')) {
            c.deleteChar();
        }
    }
    c.endEditBlock();
}

// Adds @done(today), or takes @done away again.
void Backend::toggleDone(int start, int end) {
    const auto range = blockRange(start, end);
    QTextCursor c(m_doc);
    c.beginEditBlock();
    for (int i = range.first; i <= range.second; ++i) {
        const QTextBlock b = m_doc->findBlockByNumber(i);
        const Item item = parseLine(b.text());
        if (item.type == Item::Blank)
            continue;
        bool removed = false;
        for (const auto &tag : item.tags) {
            if (tag.name.compare("done", Qt::CaseInsensitive) != 0)
                continue;
            const int from = tag.start > 0 ? tag.start - 1 : tag.start; // the space before it
            c.setPosition(b.position() + from);
            c.setPosition(b.position() + tag.start + tag.length, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            removed = true;
            break;
        }
        if (!removed) {
            c.setPosition(b.position() + b.length() - 1);
            c.insertText(" @done(" + QDate::currentDate().toString(Qt::ISODate) + ")");
        }
    }
    c.endEditBlock();
}

// Deletes whole lines. A folded item takes its hidden contents with it, since they can't be seen to be kept.
void Backend::deleteLines(int start, int end) {
    if (!m_doc)
        return;
    const auto range = blockRange(start, end);
    int last = range.second;
    for (int i = range.first; i <= range.second; ++i)
        if (foldIndex(i) >= 0)
            last = qMax(last, foldEnd(i) - 1);
    const QTextBlock from = m_doc->findBlockByNumber(range.first), to = m_doc->findBlockByNumber(last);
    QTextCursor c(m_doc);
    if (to.next().isValid()) {
        c.setPosition(from.position());
        c.setPosition(to.next().position(), QTextCursor::KeepAnchor);
    } else {
        // The last line has no newline after it, so take the one before it.
        c.setPosition(from.position() > 0 ? from.position() - 1 : 0);
        c.setPosition(to.position() + to.length() - 1, QTextCursor::KeepAnchor);
    }
    c.removeSelectedText();
}

// Moves items under an "Archive:" project at the bottom of the file, the way TaskPaper does: they go in at
// the top of the project, keep their children, tags and @done, and gain @project(Parent / Child) naming
// where they came from. With nothing selected it archives every @done item, as TaskPaper's command does.
void Backend::archive(int start, int end) {
    if (!m_doc)
        return;
    const int count = m_doc->blockCount();
    QVector<Item> items;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next())
        items.append(parseLine(b.text()));

    int archiveBlock = -1, archiveEnd = -1;
    for (int i = 0; i < count && archiveBlock < 0; ++i)
        if (items[i].type == Item::Project && items[i].indent == 0 && items[i].title == "Archive") {
            archiveBlock = i;
            archiveEnd = subtreeEnd(i);
        }

    // Which items to move: tops of subtrees only, never anything already archived.
    QList<QPair<int, int>> chunks; // [first, last] lines
    int skipUntil = 0;
    int first = 0, last = count - 1;
    const bool selection = end > start;
    if (selection) {
        const auto range = blockRange(start, end);
        first = range.first;
        last = range.second;
    }
    for (int i = first; i <= last; ++i) {
        if (i < skipUntil || items[i].type == Item::Blank || (archiveBlock >= 0 && i >= archiveBlock && i < archiveEnd))
            continue;
        if (!selection && !items[i].hasTag("done"))
            continue;
        int stop = subtreeEnd(i);
        while (stop > i + 1 && items[stop - 1].type == Item::Blank)
            --stop; // trailing blank lines stay behind
        chunks.append({i, stop - 1});
        skipUntil = stop;
    }
    if (chunks.isEmpty())
        return;

    // Build the moved text first: one level in, with @project(...) on each item's top line.
    QStringList moved;
    for (const auto &chunk : chunks) {
        const Item &top = items[chunk.first];
        QStringList path;
        for (int p = parentBlock(chunk.first); p >= 0; p = parentBlock(p))
            if (items[p].type == Item::Project)
                path.prepend(items[p].title);
        const int delta = 1 - top.level;
        for (int j = chunk.first; j <= chunk.second; ++j) {
            QString line = m_doc->findBlockByNumber(j).text();
            if (delta > 0)
                line.prepend(QString(delta, '\t'));
            else if (delta < 0)
                line.remove(0, qMin(-delta, items[j].level));
            if (j == chunk.first && !path.isEmpty() && !top.hasTag("project"))
                line += " @project(" + path.join(" / ") + ")";
            moved << line;
        }
    }

    QTextCursor c(m_doc);
    c.beginEditBlock();
    for (int k = chunks.size() - 1; k >= 0; --k) { // from the bottom, so earlier line numbers stay valid
        const QTextBlock from = m_doc->findBlockByNumber(chunks[k].first), to = m_doc->findBlockByNumber(chunks[k].second);
        if (to.next().isValid()) {
            c.setPosition(from.position());
            c.setPosition(to.next().position(), QTextCursor::KeepAnchor);
        } else {
            // The last line has no newline after it, so take the one before it.
            c.setPosition(from.position() > 0 ? from.position() - 1 : 0);
            c.setPosition(to.position() + to.length() - 1, QTextCursor::KeepAnchor);
        }
        c.removeSelectedText();
    }

    int at = -1;
    for (QTextBlock b = m_doc->begin(); b.isValid() && at < 0; b = b.next()) {
        const Item item = parseLine(b.text());
        if (item.type == Item::Project && item.indent == 0 && item.title == "Archive")
            at = b.blockNumber();
    }
    if (at < 0) {
        c.movePosition(QTextCursor::End);
        if (!m_doc->lastBlock().text().isEmpty())
            c.insertText("\n");
        c.insertText("Archive:");
        at = m_doc->lastBlock().blockNumber();
    }
    const QTextBlock archiveLine = m_doc->findBlockByNumber(at);
    c.setPosition(archiveLine.position() + archiveLine.length() - 1);
    c.insertText("\n" + moved.join("\n"));
    c.endEditBlock();
}

// Moves the selected lines, with their children, past the neighbouring item at the same level.
// The projects a selection could be moved into: every project except those inside the selection itself,
// the Searches project, and saved searches.
QVariantList Backend::moveTargets(int start, int end) const {
    QVariantList targets;
    if (!m_doc)
        return targets;
    const auto range = blockRange(start, end);
    const int chunkEnd = subtreeEnd(range.second);
    int searchesEnd = -1;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next()) {
        const int n = b.blockNumber();
        const Item item = parseLine(b.text());
        if (item.type == Item::Project && item.indent == 0 && item.title == "Searches")
            searchesEnd = subtreeEnd(n);
        if (item.type != Item::Project || item.hasTag("search") || (searchesEnd > n))
            continue;
        if (n >= range.first && n < chunkEnd)
            continue;
        targets << QVariantMap{{"label", item.title}, {"level", item.indent / 4}, {"block", n}};
    }
    return targets;
}

// Moves the selected items, and everything beneath them, to the end of a project, indented to sit inside it.
void Backend::moveToProject(int start, int end, int targetBlock, int caret) {
    if (!m_doc || !m_doc->findBlockByNumber(targetBlock).isValid())
        return;
    const auto range = blockRange(start, end);
    const int first = range.first;
    int last = subtreeEnd(range.second);
    while (last > first + 1 && parseLine(m_doc->findBlockByNumber(last - 1).text()).type == Item::Blank)
        --last; // spacing after the items stays behind
    if (targetBlock >= first && targetBlock < last)
        return; // into itself

    // After the target's last real line, so trailing blank lines stay below the moved items.
    int after = subtreeEnd(targetBlock);
    while (after > targetBlock + 1 && parseLine(m_doc->findBlockByNumber(after - 1).text()).type == Item::Blank)
        --after;
    const Item target = parseLine(m_doc->findBlockByNumber(targetBlock).text());

    int smallest = -1;
    for (int i = first; i < last; ++i) {
        const Item item = parseLine(m_doc->findBlockByNumber(i).text());
        if (item.type != Item::Blank && (smallest < 0 || item.indent < smallest))
            smallest = item.indent;
    }
    const int delta = target.indent + 4 - qMax(smallest, 0); // columns to shift everything by
    auto shifted = [&](QString line) {
        if (line.trimmed().isEmpty())
            return line;
        if (delta >= 0)
            return QString(delta / 4, '\t') + QString(delta % 4, ' ') + line;
        int remove = -delta, i = 0;
        while (i < line.size() && remove > 0 && (line[i] == '\t' || line[i] == ' ')) {
            remove -= line[i] == '\t' ? 4 : 1;
            ++i;
        }
        return line.mid(i);
    };
    auto text = [&](int from, int to, bool shift) {
        QStringList lines;
        for (int i = from; i < to; ++i) {
            const QString line = m_doc->findBlockByNumber(i).text();
            lines << (shift ? shifted(line) : line);
        }
        return lines;
    };

    // Rewrite the span between the two places in one edit, so a single Undo puts everything back.
    const QStringList moving = text(first, last, true);
    QStringList rewritten;
    int a, b, newFirst;
    if (after >= last) {
        a = first;
        b = after;
        rewritten = text(last, after, false) + moving;
        newFirst = after - (last - first);
    } else {
        a = after;
        b = last;
        rewritten = moving + text(after, first, false);
        newFirst = after;
    }
    // Nothing opens or closes: folds are remembered by line and put back on the lines' new positions after the rewrite.
    const int lenMoving = last - first;
    QList<int> foldedNow;
    for (int f = m_folds.size() - 1; f >= 0; --f) {
        const int fb = m_folds[f].blockNumber();
        if (fb < a || fb >= b)
            continue;
        if (after >= last)
            foldedNow << (fb >= first && fb < last ? newFirst + (fb - first) : a + (fb - last));
        else
            foldedNow << (fb >= first && fb < last ? newFirst + (fb - first) : a + lenMoving + (fb - after));
        m_folds.removeAt(f);
    }

    // The caret goes to the same place in the moved item: same line, same spot in the text.
    int caretLine = -1, caretColumn = 0, indentChange = 0;
    if (caret >= 0) {
        const QTextBlock cb = m_doc->findBlock(caret);
        if (cb.isValid() && cb.blockNumber() >= first && cb.blockNumber() < last) {
            caretLine = cb.blockNumber() - first;
            caretColumn = caret - cb.position();
            indentChange = shifted(cb.text()).size() - cb.text().size();
        }
    }

    QTextCursor c(m_doc->findBlockByNumber(a));
    c.setPosition(m_doc->findBlockByNumber(b - 1).position() + m_doc->findBlockByNumber(b - 1).length() - 1, QTextCursor::KeepAnchor);
    c.insertText(rewritten.join("\n"));
    for (int fb : foldedNow)
        m_folds.append(QTextCursor(m_doc->findBlockByNumber(fb)));
    refresh();
    if (caretLine >= 0) {
        const QTextBlock nb = m_doc->findBlockByNumber(newFirst + caretLine);
        const int position = nb.position() + qBound(0, caretColumn + indentChange, qMax(nb.length() - 1, 0));
        emit cursorRequested(position);
        keepCaretVisible(position); // into a folded project: the caret waits on the folded line instead
    }
}

void Backend::moveLines(int start, int end, int direction) {
    const auto range = blockRange(start, end);
    // Blank lines after an item stay where they are; they are spacing, not part of what moves.
    auto trimmed = [&](int exclusiveEnd, int firstLine) {
        while (exclusiveEnd > firstLine + 1 && parseLine(m_doc->findBlockByNumber(exclusiveEnd - 1).text()).type == Item::Blank)
            --exclusiveEnd;
        return exclusiveEnd;
    };
    const int first = range.first, fullEnd = subtreeEnd(range.second);
    const int last = trimmed(fullEnd, first); // [first, last)
    const int level = parseLine(m_doc->findBlockByNumber(first).text()).indent;
    int otherFirst, otherLast;
    if (direction < 0) {
        // The previous sibling is the nearest line above at this item's level, skipping over its children
        // (folded or not). Reaching a shallower line first means there is no sibling: the item stays put.
        otherFirst = -1;
        for (int i = first - 1; i >= 0; --i) {
            const Item item = parseLine(m_doc->findBlockByNumber(i).text());
            if (item.type == Item::Blank || item.indent > level) continue;
            if (item.indent == level) otherFirst = i;
            break;
        }
        if (otherFirst < 0) return;
        otherLast = first;
    } else {
        const int sibling = fullEnd;
        if (!m_doc->findBlockByNumber(sibling).isValid()
            || parseLine(m_doc->findBlockByNumber(sibling).text()).indent != level
            || parseLine(m_doc->findBlockByNumber(sibling).text()).type == Item::Blank)
            return;
        otherFirst = last; // the blank lines between the two travel with the sibling
        otherLast = trimmed(subtreeEnd(sibling), sibling);
    }
    const int a = qMin(first, otherFirst), b = qMax(last, otherLast); // the two chunks span [a, b)
    auto text = [&](int from, int to) {
        QStringList lines;
        for (int i = from; i < to; ++i) lines << m_doc->findBlockByNumber(i).text();
        return lines.join("\n");
    };
    const QString mine = text(first, last), other = text(otherFirst, otherLast);
    const QString swapped = direction < 0 ? mine + "\n" + other : other + "\n" + mine;

    // Folds travel with their items: note which lines in the span are folded, and put the folds back on
    // the lines' new positions after the rewrite, so moving never opens or closes anything.
    const int lenMine = last - first, lenOther = otherLast - otherFirst;
    const int newStart = direction < 0 ? a : a + lenOther;
    const int otherStart = direction < 0 ? a + lenMine : a;
    QList<int> foldedNow;
    for (int f = m_folds.size() - 1; f >= 0; --f) {
        const int fb = m_folds[f].blockNumber();
        if (fb < a || fb >= b) continue;
        foldedNow << (fb >= first && fb < last ? newStart + (fb - first) : otherStart + (fb - otherFirst));
        m_folds.removeAt(f);
    }

    QTextCursor c(m_doc->findBlockByNumber(a));
    c.setPosition(m_doc->findBlockByNumber(b - 1).position() + m_doc->findBlockByNumber(b - 1).length() - 1, QTextCursor::KeepAnchor);
    c.insertText(swapped);
    for (int fb : foldedNow)
        m_folds.append(QTextCursor(m_doc->findBlockByNumber(fb)));
    refresh();
    const QTextBlock s = m_doc->findBlockByNumber(newStart);
    const QTextBlock e = m_doc->findBlockByNumber(newStart + (last - first) - 1);
    emit moved(s.position(), e.position() + e.length() - 1);
}
