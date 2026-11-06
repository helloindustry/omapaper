#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QTextCursor>
#include <QMap>
#include <QSet>
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
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE void changeFontSize(double delta); // 0 resets to the default
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveSidebarWidth();
    Q_INVOKABLE QVariantList lineRange(int position) const; // the line's own text
    Q_INVOKABLE void toggleTagFold(const QString &name);
    // More than one caret: Ctrl+click adds one, and typing then happens at every caret at once.
    Q_INVOKABLE void addCursor(int position, int primary);
    Q_INVOKABLE void clearCursors();
    Q_INVOKABLE int multiType(int primary, const QString &text);
    Q_INVOKABLE int multiKey(int primary, int key);
    Q_INVOKABLE void print(); // what is on screen, to the default printer
    bool renderPdf(const QString &path) const;
    Q_INVOKABLE void rememberCursor(int position); // reopened files put the caret back here

    Q_INVOKABLE void search(const QString &query);   // from the sidebar: searches the whole document
    Q_INVOKABLE void showProject(int block);         // from the sidebar: zooms with no search in the way
    Q_INVOKABLE void showAll();                      // back to the full document
    Q_INVOKABLE void toggleFold(int position);
    Q_INVOKABLE void toggleFoldBlock(int block);
    Q_INVOKABLE void foldAll(bool collapse);
    Q_INVOKABLE void foldItems(int start, int end, bool collapse, bool completely); // TaskPaper's Expand/Collapse Items
    Q_INVOKABLE void toggleFoldAll(); // unfolds if anything is folded, otherwise folds everything
    Q_INVOKABLE void zoomToBlock(int block);
    Q_INVOKABLE void zoomIn(int position);
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void saveSearch();
    Q_INVOKABLE void deleteBlock(int block);

    Q_INVOKABLE QString linkAt(int position) const; // the address under a character, or empty
    Q_INVOKABLE int findNext(const QString &text, int from, bool backwards); // start of the next match among visible lines, or -1
    Q_INVOKABLE int replaceRange(int start, int end, const QString &with);    // returns where the replacement ends
    Q_INVOKABLE int replaceAll(const QString &text, const QString &with);    // visible lines only; returns the count
    Q_INVOKABLE bool newline(int position);
    Q_INVOKABLE void indent(int start, int end, int direction);
    Q_INVOKABLE void toggleDone(int start, int end);
    Q_INVOKABLE void deleteLines(int start, int end); // the lines touched by the selection, plus anything folded inside them
    Q_INVOKABLE void archive(int start, int end); // the selection, or every @done item when nothing is selected
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
    QList<QTextCursor> m_folds; // fold points, as cursors so they follow the lines when text is edited
    QTimer m_refresh, m_save, m_noticeTimer;
};
