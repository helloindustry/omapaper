#pragma once

#include <QList>
#include <QString>

// A TaskPaper document is plain text: one item per line, nested by tabs.
// "- text" is a task, "text:" is a project, anything else is a note.
// This file knows nothing about Qt widgets, so it is easy to test.
struct Item {
    enum Type { Blank, Project, Task, Note };
    struct Tag {
        QString name, value;
        bool hasValue = false;
        int start = 0, length = 0; // position in the line, including the "@"
    };

    Type type = Blank;
    int level = 0;     // leading tabs
    int indent = 0;    // width of all leading whitespace (a tab is 4 columns); this alone decides nesting
    int bodyStart = 0; // first character after the tabs
    int textStart = 0; // first character after "- " for tasks
    QString title;     // the item's words, without tags or a project's colon
    QList<Tag> tags;

    bool hasTag(const QString &name) const;
    QString tagValue(const QString &name) const;
};

Item parseLine(const QString &line);

// A web, mail, or file address inside a line, so it can be underlined and clicked.
struct Link {
    int start = 0, length = 0;
    QString url; // what to open; "www." addresses get https://
};
QList<Link> findLinks(const QString &line);

// Whether `next` (a non-blank item after `root`) falls outside root's children.
// Children are normally the deeper-indented lines. A project also keeps the plain
// tasks and notes that follow it at its own level, so a list typed without
// indenting still folds, zooms, and filters as one project until the next project.
inline bool endsSubtree(const Item &root, const Item &next) {
    if (next.indent != root.indent)
        return next.indent < root.indent;
    return !(root.type == Item::Project && next.type != Item::Project);
}

// A search such as `@due < today and not @done`, `project Inbox`, or `milk or eggs`.
class Query {
public:
    static Query parse(const QString &text);
    bool isEmpty() const { return m_groups.isEmpty(); }
    bool matches(const Item &item) const;
    // With a single AND-group, an item that satisfies one of its `not` terms is filtered out along with everything
    // under it (TaskPaper needs `except @tag//*` for that; here `not @tag` does it).
    bool excludes(const Item &item) const;
    QStringList textTerms() const; // the plain words being searched for, for highlighting

private:
    struct Predicate {
        bool negate = false;
        enum Kind { Text, Tag, Type } kind = Text;
        Item::Type type = Item::Note;
        QString name, op, value; // Tag: name, op, value; Text and Type: value holds the words
    };
    bool test(const Predicate &p, const Item &item) const;

    QList<QList<Predicate>> m_groups; // OR of ANDs
};
