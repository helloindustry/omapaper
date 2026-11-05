#include "backend.h"

#include <QDate>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSettings>
#include <QTextBlock>
#include <QUrl>

#include "outline.h"

Backend::Backend(Theme *theme, const QString &initialFile, QObject *parent)
    : QObject(parent), m_theme(theme), m_initialFile(initialFile) {
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, &Backend::refresh);
    // Like TaskPaper, there is no Save command to forget: edits reach the file shortly after typing.
    m_save.setSingleShot(true);
    m_save.setInterval(800);
    connect(&m_save, &QTimer::timeout, this, &Backend::save);
    connect(m_theme, &Theme::changed, this, &Backend::applyColors);
}

QString Backend::fontFamily() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont).family(); }
QString Backend::title() const { return m_path.isEmpty() ? tr("Untitled") : QFileInfo(m_path).fileName(); }

void Backend::applyColors() {
    if (m_highlighter)
        m_highlighter->setColors(m_theme->accent(), m_theme->muted());
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
    if (path.isEmpty())
        path = settings.value("lastFile").toString();
    if (path.isEmpty())
        path = QDir::homePath() + "/Documents/Tasks.taskpaper";
    load(path);
}

void Backend::load(const QString &path) {
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
    m_zoomed = false;
    m_pin = QTextCursor();
    QSettings("taskpaper", "taskpaper").setValue("lastFile", path);
    setError({});
    emit fileChanged();
    emit zoomChanged();
    refresh();
}

void Backend::openPath(const QString &path) {
    save();
    load(path);
}

// Written beside the file and renamed, so a crash never leaves half a document.
void Backend::save() {
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

void Backend::portalResponse(uint code, const QVariantMap &results) {
    if (code != 0)
        return;
    QVariant uris = results.value("uris");
    QStringList list = uris.canConvert<QDBusArgument>() ? qdbus_cast<QStringList>(uris.value<QDBusArgument>())
                                                        : uris.toStringList();
    if (!list.isEmpty())
        openPath(QUrl(list.first()).toLocalFile());
}

void Backend::onChange(int position, int, int) {
    if (m_applying)
        return;
    m_dirty = true;
    // The line being typed stays visible even if it doesn't match the search yet.
    m_pin = QTextCursor(m_doc->findBlock(position));
    m_refresh.start();
    m_save.start();
}

// ---- filtering, zooming, sidebar -------------------------------------------------

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
        while (!stack.isEmpty() && items[stack.last()].level >= items[i].level)
            end[stack.takeLast()] = i;
        parent[i] = stack.isEmpty() ? -1 : stack.last();
        stack.append(i);
    }

    // The Searches project holds saved searches; it is not a project or a result.
    QVector<bool> inSearches(n, false);
    for (int i = 0; i < n; ++i)
        if (items[i].type == Item::Project && items[i].level == 0 && items[i].title == "Searches")
            for (int j = i; j < end[i]; ++j)
                inSearches[j] = true;

    QVector<bool> visible(n, true);
    const Query query = Query::parse(m_query);
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
    if (m_zoomed) {
        const int root = m_zoom.blockNumber();
        for (int i = 0; i < n; ++i)
            visible[i] = visible[i] && i >= root && i < end[root];
        if (root < n)
            visible[root] = true;
    }
    if (!m_pin.isNull() && m_pin.blockNumber() < n)
        visible[m_pin.blockNumber()] = true;

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

    QVariantList projects, tags, searches;
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
            projects << QVariantMap{{"kind", "project"}, {"label", item.title}, {"level", item.level}, {"block", i}};
        for (const auto &tag : item.tags)
            counts[tag.name]++;
    }
    for (auto it = counts.begin(); it != counts.end(); ++it)
        tags << QVariantMap{{"kind", "tag"}, {"label", "@" + it.key()}, {"query", "@" + it.key()}, {"count", it.value()}};

    QVariantList sidebar;
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
}

int Backend::parentBlock(int block) const {
    const int level = parseLine(m_doc->findBlockByNumber(block).text()).level;
    for (QTextBlock b = m_doc->findBlockByNumber(block).previous(); b.isValid(); b = b.previous()) {
        const Item item = parseLine(b.text());
        if (item.type != Item::Blank && item.level < level)
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

void Backend::zoomToBlock(int block) {
    if (!m_doc || !m_doc->findBlockByNumber(block).isValid())
        return;
    m_zoom = QTextCursor(m_doc->findBlockByNumber(block));
    m_zoomed = true;
    m_pin = QTextCursor();
    refresh();
}

void Backend::zoomIn(int position) {
    const QTextBlock b = m_doc->findBlock(position);
    if (parseLine(b.text()).type != Item::Blank)
        zoomToBlock(b.blockNumber());
}

void Backend::clearZoom() {
    m_zoomed = false;
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
        if (item.type == Item::Project && item.level == 0 && item.title == "Searches") { at = b.blockNumber(); break; }
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
    const int level = parseLine(m_doc->findBlockByNumber(block).text()).level;
    int i = block + 1;
    for (QTextBlock b = m_doc->findBlockByNumber(i); b.isValid(); b = b.next(), ++i) {
        const Item item = parseLine(b.text());
        if (item.type != Item::Blank && item.level <= level)
            break;
    }
    return i;
}

// Enter continues a list: a new task under a task, the same indent otherwise.
bool Backend::newline(int position) {
    const QTextBlock b = m_doc->findBlock(position);
    const Item item = parseLine(b.text());
    const int column = position - b.position();
    if (item.type == Item::Blank || column < item.textStart)
        return false;
    QTextCursor c(m_doc);
    c.beginEditBlock();
    if (item.type == Item::Task && item.title.isEmpty() && item.tags.isEmpty()) {
        // Enter on an empty task ends the list.
        c.setPosition(b.position() + item.bodyStart);
        c.setPosition(b.position() + b.length() - 1, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    } else {
        c.setPosition(position);
        c.insertText("\n" + QString(item.level, '\t') + (item.type == Item::Task ? "- " : ""));
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
            if (!b.text().isEmpty())
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

// Moves the selected lines, with their children, past the neighbouring item at the same level.
void Backend::moveLines(int start, int end, int direction) {
    const auto range = blockRange(start, end);
    const int first = range.first, last = subtreeEnd(range.second); // [first, last)
    const int level = parseLine(m_doc->findBlockByNumber(first).text()).level;
    int otherFirst, otherLast;
    if (direction < 0) {
        otherFirst = -1;
        for (int i = first - 1; i >= 0; --i) {
            const Item item = parseLine(m_doc->findBlockByNumber(i).text());
            if (item.type == Item::Blank) continue;
            if (item.level == level) otherFirst = i;
            break;
        }
        if (otherFirst < 0) return;
        otherLast = first;
    } else {
        otherFirst = last;
        if (!m_doc->findBlockByNumber(otherFirst).isValid()
            || parseLine(m_doc->findBlockByNumber(otherFirst).text()).level != level
            || parseLine(m_doc->findBlockByNumber(otherFirst).text()).type == Item::Blank)
            return;
        otherLast = subtreeEnd(otherFirst);
    }
    const int a = qMin(first, otherFirst), b = qMax(last, otherLast); // the two chunks span [a, b)
    auto text = [&](int from, int to) {
        QStringList lines;
        for (int i = from; i < to; ++i) lines << m_doc->findBlockByNumber(i).text();
        return lines.join("\n");
    };
    const QString mine = text(first, last), other = text(otherFirst, otherLast);
    const QString swapped = direction < 0 ? mine + "\n" + other : other + "\n" + mine;

    QTextCursor c(m_doc->findBlockByNumber(a));
    c.setPosition(m_doc->findBlockByNumber(b - 1).position() + m_doc->findBlockByNumber(b - 1).length() - 1, QTextCursor::KeepAnchor);
    c.insertText(swapped);
    const int newStart = direction < 0 ? a : a + other.count('\n') + 1;
    const QTextBlock s = m_doc->findBlockByNumber(newStart);
    const QTextBlock e = m_doc->findBlockByNumber(newStart + (last - first) - 1);
    emit moved(s.position(), e.position() + e.length() - 1);
}
