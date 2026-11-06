#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QTextCursor>
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
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList sidebar READ sidebar NOTIFY sidebarChanged)
    Q_PROPERTY(QVariantList folds READ folds NOTIFY foldsChanged)
    Q_PROPERTY(int zoomBlock READ zoomBlock NOTIFY zoomChanged)
    Q_PROPERTY(QString zoomPath READ zoomPath NOTIFY zoomChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Backend(Theme *theme, const QString &initialFile = {}, QObject *parent = nullptr);

    QString title() const;
    QString fontFamily() const;
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QVariantList sidebar() const { return m_sidebar; }
    QVariantList folds() const { return m_foldMarks; }
    int zoomBlock() const { return m_zoomed ? m_zoom.blockNumber() : -1; }
    QString zoomPath() const;
    QString error() const { return m_error; }

    Q_INVOKABLE void attach(QQuickTextDocument *document);
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void save();

    Q_INVOKABLE void search(const QString &query);   // from the sidebar: searches the whole document
    Q_INVOKABLE void showProject(int block);         // from the sidebar: zooms with no search in the way
    Q_INVOKABLE void showAll();                      // back to the full document
    Q_INVOKABLE void toggleFold(int position);
    Q_INVOKABLE void toggleFoldBlock(int block);
    Q_INVOKABLE void foldAll(bool collapse);
    Q_INVOKABLE void zoomToBlock(int block);
    Q_INVOKABLE void zoomIn(int position);
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void saveSearch();
    Q_INVOKABLE void deleteBlock(int block);

    Q_INVOKABLE QString linkAt(int position) const; // the address under a character, or empty
    Q_INVOKABLE bool newline(int position);
    Q_INVOKABLE void indent(int start, int end, int direction);
    Q_INVOKABLE void toggleDone(int start, int end);
    Q_INVOKABLE void moveLines(int start, int end, int direction);

signals:
    void fileChanged();
    void queryChanged();
    void sidebarChanged();
    void zoomChanged();
    void errorChanged();
    void moved(int start, int end);
    void foldsChanged();
    void cursorRequested(int position);

private slots:
    void portalResponse(uint code, const QVariantMap &results);

private:
    void load(const QString &path);
    void refresh();
    void onChange(int position, int removed, int added);
    void applyColors();
    void setError(const QString &message);
    void clearZoom();
    int parentBlock(int block) const;
    QPair<int, int> blockRange(int start, int end) const;
    int subtreeEnd(int block) const;
    bool hasChildren(int block) const;
    int foldIndex(int block) const;

    Theme *m_theme;
    QString m_initialFile, m_path, m_query, m_error;
    QTextDocument *m_doc = nullptr;
    Highlighter *m_highlighter = nullptr;
    QTextCursor m_zoom, m_pin; // cursors, so they follow the lines as text is edited
    bool m_zoomed = false, m_applying = false, m_dirty = false;
    QVariantList m_sidebar, m_foldMarks;
    QList<QTextCursor> m_folds; // fold points, as cursors so they follow the lines when text is edited
    QTimer m_refresh, m_save;
};
