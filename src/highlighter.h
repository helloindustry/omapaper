#pragma once

#include <QColor>
#include <QSyntaxHighlighter>

// Styles each line as it is typed: bold projects, dimmed notes, struck-through
// done tasks, and colored tags. The text itself is never changed.
class Highlighter : public QSyntaxHighlighter {
public:
    explicit Highlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
    void setColors(const QColor &accent, const QColor &muted);

protected:
    void highlightBlock(const QString &text) override;

private:
    QColor m_accent = "#FFD60A", m_muted = "#888888";
};
