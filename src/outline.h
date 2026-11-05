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
    int bodyStart = 0; // first character after the tabs
    int textStart = 0; // first character after "- " for tasks
    QString title;     // the item's words, without tags or a project's colon
    QList<Tag> tags;

    bool hasTag(const QString &name) const;
    QString tagValue(const QString &name) const;
};

Item parseLine(const QString &line);

// A search such as `@due < today and not @done`, `project Inbox`, or `milk or eggs`.
class Query {
public:
    static Query parse(const QString &text);
    bool isEmpty() const { return m_groups.isEmpty(); }
    bool matches(const Item &item) const;

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
