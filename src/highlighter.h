#pragma once

#include <QColor>
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QSyntaxHighlighter>

// Styles each line as it is typed: bold projects, dimmed notes, struck-through
// done tasks, and colored tags. The text itself is never changed.
class Highlighter : public QSyntaxHighlighter {
public:
    explicit Highlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
    void setColors(const QColor &accent, const QColor &muted);
    // Underlines the characters [start, end) of the document while the pointer is over a tag or link; an empty span clears it.
    void setHover(int start, int end);
    // Words of the current search; every occurrence in the editor is drawn in the accent colour.
    void setSearchTerms(const QStringList &terms);

protected:
    void highlightBlock(const QString &text) override;
    void markdown(const QString &text, int from, const QVector<QPair<int, int>> &skip);

private:
    QStringList m_terms;
    int m_hoverStart = -1, m_hoverEnd = -1;
    QColor m_accent = "#FFD60A", m_muted = "#888888";
};
