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

// Dates compare as dates, numbers as numbers, everything else as lowercase text.
static QDateTime toDate(QString s) {
    s = s.trimmed().toLower();
    const QDate today = QDate::currentDate();
    if (s == "today") return QDateTime(today, QTime(0, 0));
    if (s == "tomorrow") return QDateTime(today.addDays(1), QTime(0, 0));
    if (s == "yesterday") return QDateTime(today.addDays(-1), QTime(0, 0));
    for (const char *format : {"yyyy-MM-dd HH:mm", "yyyy-MM-dd"}) {
        const auto d = QDateTime::fromString(s, format);
        if (d.isValid()) return d;
    }
    return {};
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
