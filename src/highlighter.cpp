#include <functional>
#include "highlighter.h"

#include "outline.h"

void Highlighter::setColors(const QColor &accent, const QColor &muted) {
    m_accent = accent;
    m_muted = muted;
    rehighlight();
}

void Highlighter::setSearchTerms(const QStringList &terms) {
    if (terms == m_terms)
        return;
    m_terms = terms;
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

// Light Markdown, only ever noticed by someone who types it: **bold**, *italic*, _italic_, ***both***,
// ~~strike~~ and `code`. Markers need a non-space inside and a word edge outside, so prices, "2*3*4",
// snake_case, URLs and @tags in ordinary TaskPaper lines are never touched.
void Highlighter::markdown(const QString &text, int from, const QVector<QPair<int, int>> &skip) {
    const int n = text.size();
    QVector<bool> blocked(n, false);
    for (const auto &r : skip)
        for (int i = r.first; i < qMin(r.first + r.second, n); ++i)
            blocked[i] = true;
    auto word = [&](int i) { return i >= 0 && i < n && text[i].isLetterOrNumber(); };
    auto space = [&](int i) { return i < 0 || i >= n || text[i].isSpace(); };
    auto edit = [&](int a, int len, const std::function<void(QTextCharFormat &)> &change) {
        for (int i = a; i < a + len && i < n; ++i) {
            QTextCharFormat f = format(i);
            change(f);
            setFormat(i, 1, f);
        }
    };
    QTextCharFormat dim;
    dim.setForeground(m_muted);

    // `code` first: nothing inside it is read as Markdown.
    for (int i = from; i < n; ++i) {
        if (text[i] != '`' || blocked[i])
            continue;
        int j = text.indexOf('`', i + 1);
        if (j < 0)
            break;
        if (j > i + 1) {
            bool clear = true;
            for (int k = i; k <= j; ++k)
                clear = clear && !blocked[k];
            if (clear) {
                edit(i + 1, j - i - 1, [&](QTextCharFormat &f) {
                    f.setFontFamilies({QStringLiteral("monospace")});
                    f.setBackground(QColor(128, 128, 128, 45));
                });
                edit(i, 1, [&](QTextCharFormat &f) { f.setForeground(m_muted); });
                edit(j, 1, [&](QTextCharFormat &f) { f.setForeground(m_muted); });
                for (int k = i; k <= j; ++k)
                    blocked[k] = true;
            }
        }
        i = j;
    }

    struct Rule { QString mark; bool edges; std::function<void(QTextCharFormat &)> style; };
    const QList<Rule> rules = {
        {"***", true, [](QTextCharFormat &f) { f.setFontWeight(QFont::Bold); f.setFontItalic(true); }},
        {"**", true, [](QTextCharFormat &f) { f.setFontWeight(QFont::Bold); }},
        {"__", true, [](QTextCharFormat &f) { f.setFontWeight(QFont::Bold); }},
        {"*", true, [](QTextCharFormat &f) { f.setFontItalic(true); }},
        {"_", true, [](QTextCharFormat &f) { f.setFontItalic(true); }},
        {"~~", false, [](QTextCharFormat &f) { f.setFontStrikeOut(true); }},
    };
    for (const Rule &rule : rules) {
        const int m = rule.mark.size();
        const QChar c = rule.mark[0];
        auto isMark = [&](int i) {
            if (i < 0 || i + m > n || blocked[i] || text.mid(i, m) != rule.mark)
                return false;
            // A single * or _ must not be part of a longer run of the same character.
            if (m == 1 || (m == 2 && c != '~')) {
                if (m == 1 && ((i > 0 && text[i - 1] == c) || (i + 1 < n && text[i + 1] == c)))
                    return false;
                if (m == 2 && ((i > 0 && text[i - 1] == c) || (i + 2 < n && text[i + 2] == c)))
                    return false;
            }
            return true;
        };
        for (int i = from; i < n; ++i) {
            if (!isMark(i) || space(i + m) || (rule.edges && word(i - 1)))
                continue;
            int j = i + m + 1;
            for (; j < n; ++j)
                if (isMark(j) && !space(j - 1) && !(rule.edges && word(j + m)))
                    break;
            if (j >= n)
                continue;
            edit(i + m, j - i - m, rule.style);
            edit(i, m, [&](QTextCharFormat &f) { f.setForeground(m_muted); });
            edit(j, m, [&](QTextCharFormat &f) { f.setForeground(m_muted); });
            i = j + m - 1;
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

    QVector<QPair<int, int>> skip;
    for (const auto &link : findLinks(text))
        skip.append({link.start, link.length});
    for (const auto &tag : item.tags)
        skip.append({tag.start, tag.length});
    markdown(text, item.type == Item::Task ? item.textStart : item.bodyStart, skip);

    for (const QString &term : m_terms) {
        for (int at = text.indexOf(term, item.bodyStart, Qt::CaseInsensitive); at >= 0;
             at = text.indexOf(term, at + term.size(), Qt::CaseInsensitive)) {
            for (int i = at; i < at + term.size(); ++i) {
                QTextCharFormat found = format(i);
                found.setForeground(m_accent);
                setFormat(i, 1, found);
            }
        }
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
