#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QTextCursor>
#include <QMap>
#include <QSet>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QVariantList>

#include "highlighter.h"
#include "theme.h"

// The document, its file, and everything TaskPaper does to it: filtering and
// zooming by hiding lines (never by copying them), and the line editing keys.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY fileChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(double fontSize READ fontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(double sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
    Q_PROPERTY(QVariantList extraCursors READ extraCursors NOTIFY extraCursorsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList sidebar READ sidebar NOTIFY sidebarChanged)
    Q_PROPERTY(QVariantList folds READ folds NOTIFY foldsChanged)
    Q_PROPERTY(int zoomBlock READ zoomBlock NOTIFY zoomChanged)
    Q_PROPERTY(QString zoomPath READ zoomPath NOTIFY zoomChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

public:
    explicit Backend(Theme *theme, const QString &initialFile = {}, QObject *parent = nullptr);

    QString title() const;
    QString fontFamily() const;
    double fontSize() const { return m_fontSize; }
    double sidebarWidth() const { return m_sidebarWidth; }
    void setSidebarWidth(double width) { if (width != m_sidebarWidth) { m_sidebarWidth = width; emit sidebarWidthChanged(); } }
    QVariantList extraCursors() const;
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QVariantList sidebar() const { return m_sidebar; }
    QVariantList folds() const { return m_foldMarks; }
    int zoomBlock() const { return m_zoomed ? m_zoom.blockNumber() : -1; }
    QString zoomPath() const;
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }

    Q_INVOKABLE void attach(QQuickTextDocument *document);
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void newWindow(); // another copy of the app with a blank, untitled document
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE void changeFontSize(double delta); // 0 resets to the default
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void save();
    void setStartBlank(bool blank) { m_startBlank = blank; } // an untitled document instead of the last file
    void keepRescueCopy();
    void saveOnQuit(); // save, and keep a rescue copy if the file can't be written
    QString readOnlyMessage() const;
    QVariantList projectList(int skipFrom, int skipTo) const;
    Q_INVOKABLE void saveNow(); // Ctrl+S: saves, and says so even when autosave had already written everything
    Q_INVOKABLE void saveSidebarWidth();
    Q_INVOKABLE bool caretThemed() const; // false means the iA blue caret; the theme's accent is the default
    Q_INVOKABLE void setCaretThemed(bool themed);
    Q_INVOKABLE QVariantList lineRange(int position) const; // the line's own text
    Q_INVOKABLE void toggleTagFold(const QString &name);
    Q_INVOKABLE bool sidebarShown() const;
    Q_INVOKABLE void setSidebarShown(bool shown);
    // More than one caret: Ctrl+click adds one, and typing then happens at every caret at once.
    Q_INVOKABLE void addCursor(int position, int primary);
    Q_INVOKABLE void clearCursors();
    Q_INVOKABLE int multiType(int primary, const QString &text);
    Q_INVOKABLE int multiKey(int primary, int key);
    Q_INVOKABLE void print(); // what is on screen, to the default printer
    bool renderPdf(const QString &path) const;
    Q_INVOKABLE void rememberCursor(int position); // reopened files put the caret back here

    Q_INVOKABLE void search(const QString &query);   // from the sidebar: searches the whole document
    Q_INVOKABLE void include(const QString &term);   // Ctrl+click: add this term to the search (again to take it out)
    Q_INVOKABLE void exclude(const QString &term);   // Ctrl+Shift+click: filter this term out (again to undo)
    Q_INVOKABLE int termState(const QString &term) const; // 1 if the search includes it, 2 if it filters it out, else 0
    Q_INVOKABLE void showProject(int block);         // from the sidebar: zooms with no search in the way
    Q_INVOKABLE void showAll();                      // back to the full document
    Q_INVOKABLE void toggleFold(int position);
    Q_INVOKABLE void toggleFoldBlock(int block);
    Q_INVOKABLE void foldAll(bool collapse, int position = -1); // position: the caret, kept where it is unless a fold hides its line
    Q_INVOKABLE void foldOthers(int position); // folds everything except the item at the caret and the items that contain it
    void keepCaretVisible(int position);
    Q_INVOKABLE bool shiftHeld() const; // Qt matches Ctrl+Shift+key to a plain Ctrl+key shortcut, so those ask
    Q_INVOKABLE void toggleWrap(int start, int end, const QString &marker); // **bold**, *italic*, ~~strike~~ on the selection or the word span at the caret
    Q_INVOKABLE void insertLink(int start, int end);                      // [label](url), taking the URL from the clipboard
    Q_INVOKABLE void foldItems(int start, int end, bool collapse, bool completely); // TaskPaper's Expand/Collapse Items
    Q_INVOKABLE void toggleFoldAll(); // unfolds if everything is folded, otherwise folds everything
    Q_INVOKABLE void zoomToBlock(int block);
    Q_INVOKABLE void zoomIn(int position);
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void saveSearch();
    Q_INVOKABLE void deleteBlock(int block);

    Q_INVOKABLE QString tagAt(int position) const;  // the search for the tag under a character, or empty
    Q_INVOKABLE QString linkAt(int position) const; // the address under a character, or empty
    Q_INVOKABLE void setHover(int start, int end); // underline a tag or link while the pointer is over it
    Q_INVOKABLE int blockPosition(int block) const; // where a line starts; the fold arrows follow it
    Q_INVOKABLE void removeTag(int position); // deletes the tag under this character, with one space beside it
    Q_INVOKABLE QVariantList tagSpan(int position) const;  // [start, end] of that tag in the document, or empty
    Q_INVOKABLE QVariantList linkSpan(int position) const; // the same for a link
    Q_INVOKABLE int findNext(const QString &text, int from, bool backwards); // start of the next match among visible lines, or -1
    Q_INVOKABLE int replaceRange(int start, int end, const QString &with);    // returns where the replacement ends
    Q_INVOKABLE int replaceAll(const QString &text, const QString &with);    // visible lines only; returns the count
    Q_INVOKABLE bool newline(int position);
    Q_INVOKABLE void indent(int start, int end, int direction);
    Q_INVOKABLE void toggleDone(int start, int end);
    Q_INVOKABLE void duplicateLines(int start, int end); // copies the lines touched, plus their children, just below them
    Q_INVOKABLE void deleteLines(int start, int end); // the lines touched by the selection, plus anything folded inside them
    Q_INVOKABLE void archive(int start, int end); // the selection, or every @done item when nothing is selected
    Q_INVOKABLE QVariantList projectTargets() const; // every project, for "go to project"
    Q_INVOKABLE QVariantList moveTargets(int start, int end) const; // projects the selected items could move into
    Q_INVOKABLE void moveToProject(int start, int end, int targetBlock, int caret = -1); // folds stay as they were; the caret keeps its place within the moved item
    Q_INVOKABLE void moveLines(int start, int end, int direction);

signals:
    void fileChanged();
    void fontSizeChanged();
    void queryChanged();
    void sidebarChanged();
    void zoomChanged();
    void errorChanged();
    void noticeChanged();
    void sidebarWidthChanged();
    void extraCursorsChanged();
    void suggestion(int start, int end); // select this range: the rest of a tag, offered as the user types it
    void moved(int start, int end);
    void foldsChanged();
    void cursorRequested(int position);

private slots:
    void portalResponse(uint code, const QVariantMap &results);
    void saveAsResponse(uint code, const QVariantMap &results);

private:
    void load(const QString &path);
    void watchFile();
    void externalChange();
    void showNotice(const QString &message, int milliseconds = 3000);
    void persistCursor();
    void refresh();
    void onChange(int position, int removed, int added);
    void applyColors();
    void setError(const QString &message);
    void clearZoom();
    int parentBlock(int block) const;
    QPair<int, int> blockRange(int start, int end) const;
    int subtreeEnd(int block) const;
    bool hasChildren(int block) const;
    int enclosingParent(int block) const; // nearest line above that is indented less, or -1
    int foldEnd(int block) const; // end of the lines indented beneath an item
    int foldIndex(int block) const;

    Theme *m_theme;
    QString m_initialFile, m_path, m_query, m_error, m_notice;
    bool m_startBlank = false;
    bool m_notUtf8 = false;
    bool m_readOnly = false;    // read-only on disk, or not UTF-8: shown, never written
    bool m_missingNoted = false;
    QTextDocument *m_doc = nullptr;
    Highlighter *m_highlighter = nullptr;
    QTextCursor m_zoom, m_pin; // cursors, so they follow the lines as text is edited
    double m_fontSize = 10, m_sidebarWidth = 240;
    QSet<QString> m_collapsedTags;
    QMap<QString, int> m_tagCounts; // every tag in the document, for autofill
    void suggestTag(int position);
    QList<QTextCursor> m_extras;
    QList<QTextCursor> allCursors(int primary) const;
    int finishMulti(QList<QTextCursor> &all);
    int m_cursor = 0;
    double m_defaultFontSize = 9, m_fontDelta = 0;
    bool m_zoomed = false, m_applying = false, m_dirty = false;
    QVariantList m_sidebar, m_foldMarks;
    QList<QTextCursor> m_refold; // items unfolded only because the view was focused into them; folded again on the way out
    void releaseRefolds();
    QList<QTextCursor> m_folds; // fold points, as cursors so they follow the lines when text is edited
    QTimer m_refresh, m_save, m_noticeTimer, m_external;
    QFileSystemWatcher m_watcher;
    QString m_disk; // what the file held when we last read or wrote it, to tell our own saves from someone else's
};
