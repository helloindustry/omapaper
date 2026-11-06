#include "highlighter.h"

#include "outline.h"

void Highlighter::setColors(const QColor &accent, const QColor &muted) {
    m_accent = accent;
    m_muted = muted;
    rehighlight();
}

void Highlighter::highlightBlock(const QString &text) {
    const Item item = parseLine(text);
    if (item.type == Item::Blank)
        return;

    const bool done = item.hasTag("done");
    QTextCharFormat dim;
    dim.setForeground(m_muted);

    if (item.type == Item::Project) {
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        setFormat(item.bodyStart, text.size() - item.bodyStart, bold);
    } else if (item.type == Item::Note) {
        setFormat(item.bodyStart, text.size() - item.bodyStart, dim);
    } else {
        setFormat(item.bodyStart, item.textStart - item.bodyStart, dim); // the dash
        if (done) {
            QTextCharFormat struck = dim;
            struck.setFontStrikeOut(true);
            setFormat(item.textStart, text.size() - item.textStart, struck);
        }
    }

    for (const auto &link : findLinks(text)) {
        QTextCharFormat linkFormat = format(link.start);
        linkFormat.setForeground(m_accent);
        linkFormat.setFontUnderline(true);
        setFormat(link.start, link.length, linkFormat);
    }

    for (const auto &tag : item.tags) {
        QTextCharFormat tagFormat = format(tag.start);
        tagFormat.setForeground(done ? m_muted : m_accent);
        tagFormat.setFontStrikeOut(false);
        setFormat(tag.start, tag.length, tagFormat);
    }
}
