#include "outline.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>

bool Item::hasTag(const QString &name) const {
    for (const auto &t : tags)
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

QString Item::tagValue(const QString &name) const {
    for (const auto &t : tags)
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
            return t.value;
    return {};
}

Item parseLine(const QString &line) {
    Item item;
    // Any leading whitespace is indentation, so tabs, spaces, and a mix of them all nest by how wide they are.
    int i = 0, width = 0, tabs = 0;
    while (i < line.size() && line[i].isSpace()) {
        if (line[i] == '\t') {
            width = (width / 4 + 1) * 4;
            ++tabs;
        } else {
            ++width;
        }
        ++i;
    }
    item.level = tabs;
    item.indent = width;
    item.bodyStart = i;
    if (line.mid(i).trimmed().isEmpty())
        return item;

    // The "@" must follow whitespace, so an email address is not a tag.
    static const QRegularExpression tagExpression(R"re((?<!\S)@([\w.\-]+)(?:\(([^)]*)\))?)re");
    auto matches = tagExpression.globalMatch(line, i);
    QString words = line.mid(i);
    while (matches.hasNext()) {
        const auto m = matches.next();
        Item::Tag tag;
        tag.name = m.captured(1);
        tag.hasValue = m.capturedStart(2) >= 0;
        tag.value = m.captured(2);
        tag.start = m.capturedStart(0);
        tag.length = m.capturedLength(0);
        item.tags.append(tag);
    }

    const QString body = line.mid(i);
    if (body.startsWith("- ") || body == "-") {
        item.type = Item::Task;
        item.textStart = qMin(i + 2, line.size());
    } else {
        item.textStart = i;
    }

    QString text = line.mid(item.textStart);
    for (int t = item.tags.size() - 1; t >= 0; --t) {
        const auto &tag = item.tags[t];
        text.remove(tag.start - item.textStart, tag.length);
    }
    text = text.trimmed();
    if (item.type != Item::Task) {
        if (text.endsWith(':')) {
            item.type = Item::Project;
            text.chop(1);
        } else {
            item.type = Item::Note;
        }
    }
    item.title = text.simplified();
    return item;
}

// Dates compare as dates, numbers as numbers, everything else as lowercase text. A date can be written the way
// TaskPaper allows: 2026-11-05, today, tomorrow, next Monday, June 3, 3:15pm, +1 week, 2 days, tomorrow 9am,
// "Nov 26 3:15 +1 day". Anything that is not entirely made of those pieces is not a date.
static QDateTime toDate(QString s) {
    s = s.trimmed().toLower().simplified();
    if (s.isEmpty())
        return {};
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();
    static const QRegularExpression year(R"(^\d{4}$)");
    if (year.match(s).hasMatch())
        return QDateTime(QDate(s.toInt(), 1, 1), QTime(0, 0));

    static const QStringList weekdays = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    static const QStringList months = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    static const QRegularExpression isoDay(R"(\G(\d{4})-(\d{1,2})-(\d{1,2})\b)");
    static const QRegularExpression isoMonth(R"(\G(\d{4})-(\d{1,2})(?![\d-]))");
    static const QRegularExpression weekday(R"(\G(?:(next|last|this)\s*)?(mon|tue|wed|thu|fri|sat|sun)[a-z]*\b)");
    static const QRegularExpression period(R"(\G(next|last|this)\s+(week|month|year)\b)");
    static const QRegularExpression month(R"(\G(?:(next|last|this)\s*)?(jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)[a-z]*\b(?:\s*(\d{1,2})(?!\d|:|\s*[ap]m\b))?)");
    static const QRegularExpression keyword(R"(\G(today|tomorrow|yesterday|now)\b)");
    static const QRegularExpression clock(R"(\G(\d{1,2})(?::(\d{2}))?\s*(am|pm)\b|\G(\d{1,2}):(\d{2})\b)");
    static const QRegularExpression offset(R"(\G([+-]?)\s*(\d+)\s*(minutes?|mins?|hours?|hrs?|h|days?|d|weeks?|w|months?|mo|years?|y)\b)");

    QDate day = today;
    bool haveDay = false, haveTime = false;
    QTime time(0, 0);
    qint64 seconds = 0;
    int addDays = 0, addMonths = 0, addYears = 0;

    int pos = 0;
    while (pos < s.size()) {
        if (s[pos] == ' ') { ++pos; continue; }
        QRegularExpressionMatch m;
        if ((m = isoDay.match(s, pos)).hasMatch()) {
            day = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
            if (!day.isValid()) return {};
            haveDay = true;
        } else if ((m = isoMonth.match(s, pos)).hasMatch()) {
            day = QDate(m.captured(1).toInt(), m.captured(2).toInt(), 1);
            if (!day.isValid()) return {};
            haveDay = true;
        } else if ((m = period.match(s, pos)).hasMatch()) {
            const int step = m.captured(1) == "next" ? 1 : m.captured(1) == "last" ? -1 : 0;
            const QString what = m.captured(2);
            if (what == "week") day = today.addDays(1 - today.dayOfWeek() + 7 * step); // ISO weeks start on Monday
            else if (what == "month") day = QDate(today.year(), today.month(), 1).addMonths(step);
            else day = QDate(today.year() + step, 1, 1);
            haveDay = true;
        } else if ((m = weekday.match(s, pos)).hasMatch()) {
            const int want = weekdays.indexOf(m.captured(2)) + 1;
            int ahead = (want - today.dayOfWeek() + 7) % 7; // 0 = today
            if (m.captured(1) == "next" && ahead == 0) ahead = 7;
            if (m.captured(1) == "last") ahead = ahead == 0 ? -7 : ahead - 7;
            day = today.addDays(ahead);
            haveDay = true;
        } else if ((m = month.match(s, pos)).hasMatch()) {
            const int number = months.indexOf(m.captured(2)) + 1;
            int y = today.year();
            const QString which = m.captured(1);
            if (which == "next" && QDate(y, number, 1) <= today) ++y;   // the first one still ahead
            else if (which == "last" && QDate(y, number, 1) >= today) --y; // the most recent one behind
            const int d = m.captured(3).isEmpty() ? 1 : m.captured(3).toInt();
            day = QDate(y, number, d);
            if (!day.isValid()) return {};
            haveDay = true;
        } else if ((m = keyword.match(s, pos)).hasMatch()) {
            const QString k = m.captured(1);
            day = k == "tomorrow" ? today.addDays(1) : k == "yesterday" ? today.addDays(-1) : today;
            if (k == "now") { time = QTime(now.time().hour(), now.time().minute()); haveTime = true; }
            haveDay = true;
        } else if ((m = clock.match(s, pos)).hasMatch()) {
            int h = m.captured(3).isEmpty() ? m.captured(4).toInt() : m.captured(1).toInt();
            const int mins = m.captured(3).isEmpty() ? m.captured(5).toInt() : m.captured(2).toInt();
            if (!m.captured(3).isEmpty()) {
                if (h < 1 || h > 12) return {};
                h = h % 12 + (m.captured(3) == "pm" ? 12 : 0);
            }
            time = QTime(h, mins);
            if (!time.isValid()) return {};
            haveTime = true;
        } else if ((m = offset.match(s, pos)).hasMatch()) {
            const int n = m.captured(2).toInt() * (m.captured(1) == "-" ? -1 : 1);
            const QString u = m.captured(3);
            if (u.startsWith("mi")) seconds += n * 60;
            else if (u == "h" || u.startsWith("ho") || u.startsWith("hr")) seconds += n * 3600;
            else if (u == "d" || u.startsWith("da")) addDays += n;
            else if (u == "w" || u.startsWith("we")) addDays += 7 * n;
            else if (u == "mo" || u.startsWith("mon")) addMonths += n;
            else addYears += n;
        } else {
            return {};
        }
        pos += m.capturedLength();
    }

    // Durations alone count from this moment; anything with a day or a time starts from that day's midnight.
    QDateTime when = haveDay || haveTime ? QDateTime(day, time) : QDateTime(today, QTime(now.time().hour(), now.time().minute()));
    return when.addYears(addYears).addMonths(addMonths).addDays(addDays).addSecs(seconds);
}

static int order(const QString &a, const QString &b) {
    const QDateTime da = toDate(a), db = toDate(b);
    if (da.isValid() && db.isValid())
        return da < db ? -1 : da > db ? 1 : 0;
    bool okA = false, okB = false;
    const double na = a.toDouble(&okA), nb = b.toDouble(&okB);
    if (okA && okB)
        return na < nb ? -1 : na > nb ? 1 : 0;
    return a.compare(b, Qt::CaseInsensitive);
}

Query Query::parse(const QString &text) {
    Query query;
    static const QRegularExpression orSplit(R"(\s+or\s+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression andSplit(R"(\s+and\s+)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tagExpression(R"(^@([\w.\-]+)\s*(<=|>=|!=|=|<|>|contains|beginswith|endswith)?\s*(.*)$)",
                                                  QRegularExpression::CaseInsensitiveOption);

    for (const auto &orPart : text.trimmed().split(orSplit, Qt::SkipEmptyParts)) {
        QList<Predicate> group;
        for (QString part : orPart.split(andSplit, Qt::SkipEmptyParts)) {
            Predicate p;
            part = part.trimmed();
            if (part.startsWith("not ", Qt::CaseInsensitive)) {
                p.negate = true;
                part = part.mid(4).trimmed();
            }
            if (part.isEmpty())
                continue;
            const auto tag = tagExpression.match(part);
            if (tag.hasMatch()) {
                p.kind = Predicate::Tag;
                p.name = tag.captured(1);
                p.op = tag.captured(2).toLower();
                p.value = tag.captured(3).trimmed();
                if (p.value.size() >= 2 && p.value.startsWith('"') && p.value.endsWith('"'))
                    p.value = p.value.mid(1, p.value.size() - 2);
            } else {
                const QString first = part.section(' ', 0, 0).toLower();
                const QString rest = part.section(' ', 1).trimmed();
                if (first == "project" || first == "task" || first == "note") {
                    p.kind = Predicate::Type;
                    p.type = first == "project" ? Item::Project : first == "task" ? Item::Task : Item::Note;
                    p.value = rest;
                } else {
                    p.value = part;
                }
            }
            group.append(p);
        }
        if (!group.isEmpty())
            query.m_groups.append(group);
    }
    return query;
}

QStringList Query::textTerms() const {
    QStringList words;
    for (const auto &group : m_groups)
        for (const auto &p : group)
            if (p.kind == Predicate::Text && !p.negate)
                words << p.value.split(' ', Qt::SkipEmptyParts);
    words.removeDuplicates();
    return words;
}

bool Query::test(const Predicate &p, const Item &item) const {
    if (p.kind == Predicate::Tag) {
        if (!item.hasTag(p.name))
            return p.op == "!=";
        if (p.op.isEmpty())
            return true;
        const QString have = item.tagValue(p.name);
        if (p.op == "contains") return have.contains(p.value, Qt::CaseInsensitive);
        if (p.op == "beginswith") return have.startsWith(p.value, Qt::CaseInsensitive);
        if (p.op == "endswith") return have.endsWith(p.value, Qt::CaseInsensitive);
        const int c = order(have, p.value);
        if (p.op == "=") return c == 0;
        if (p.op == "!=") return c != 0;
        if (p.op == "<") return c < 0;
        if (p.op == ">") return c > 0;
        if (p.op == "<=") return c <= 0;
        return c >= 0;
    }
    if (p.kind == Predicate::Type)
        return item.type == p.type && (p.value.isEmpty() || item.title.contains(p.value, Qt::CaseInsensitive));
    for (const auto &word : p.value.split(' ', Qt::SkipEmptyParts))
        if (!item.title.contains(word, Qt::CaseInsensitive))
            return false;
    return true;
}

bool Query::matches(const Item &item) const {
    for (const auto &group : m_groups) {
        bool all = true;
        for (const auto &p : group)
            if (test(p, item) == p.negate) { all = false; break; }
        if (all)
            return true;
    }
    return false;
}

QList<Link> findLinks(const QString &line) {
    static const QRegularExpression expression(R"re((?:(?:https?|ftp|file|mailto):|(?<![\w.])www\.)[^\s<>"]+)re",
                                               QRegularExpression::CaseInsensitiveOption);
    QList<Link> links;
    auto matches = expression.globalMatch(line);
    while (matches.hasNext()) {
        const auto m = matches.next();
        QString text = m.captured(0);
        // Punctuation that ends a sentence, or closes one, isn't part of the address.
        while (!text.isEmpty() && QString(".,;:!?'\u201d").contains(text.back()))
            text.chop(1);
        if (text.endsWith(')') && !text.contains('('))
            text.chop(1);
        if (text.size() < 5)
            continue;
        Link link;
        link.start = m.capturedStart(0);
        link.length = text.size();
        link.url = text.startsWith("www.", Qt::CaseInsensitive) ? "https://" + text : text;
        links.append(link);
    }
    return links;
}

bool Query::excludes(const Item &item) const {
    if (m_groups.size() != 1)
        return false;
    for (const auto &p : m_groups.first())
        if (p.negate && test(p, item))
            return true;
    return false;
}
