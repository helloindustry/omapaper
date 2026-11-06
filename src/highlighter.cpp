#include "highlighter.h"

#include "outline.h"

void Highlighter::setColors(const QColor &accent, const QColor &muted) {
    m_accent = accent;
    m_muted = muted;
    rehighlight();
}

void Highlighter::setHover(int start, int end) {
    if (start == m_hoverStart && end == m_hoverEnd)
        return;
    const int oldStart = m_hoverStart, oldEnd = m_hoverEnd;
    m_hoverStart = start;
    m_hoverEnd = end;
    // Only the lines the span touched need restyling, before and after.
    for (const auto &span : {qMakePair(oldStart, oldEnd), qMakePair(start, end)}) {
        if (span.first < 0)
            continue;
        QTextBlock b = document()->findBlock(span.first);
        const QTextBlock last = document()->findBlock(qMax(span.first, span.second - 1));
        for (; b.isValid(); b = b.next()) {
            rehighlightBlock(b);
            if (b == last)
                break;
        }
    }
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
        setFormat(link.start, link.length, linkFormat);
    }

    for (const auto &tag : item.tags) {
        QTextCharFormat tagFormat = format(tag.start);
        tagFormat.setForeground(done ? m_muted : m_accent);
        tagFormat.setFontStrikeOut(false);
        setFormat(tag.start, tag.length, tagFormat);
    }

    if (m_hoverStart >= 0) {
        const int base = currentBlock().position();
        const int from = qMax(m_hoverStart - base, 0), to = qMin(m_hoverEnd - base, int(text.size()));
        for (int i = from; i < to; ++i) {
            QTextCharFormat underlined = format(i);
            underlined.setFontUnderline(true);
            setFormat(i, 1, underlined);
        }
    }
}
