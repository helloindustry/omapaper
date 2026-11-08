#include <QtTest>

#include "outline.h"

class TaskPaperTests : public QObject {
    Q_OBJECT

private slots:
    void readsTypes()
    {
        QCOMPARE(parseLine("- buy milk").type, Item::Task);
        QCOMPARE(parseLine("\tGroceries: @home").type, Item::Project);
        QCOMPARE(parseLine("\tGroceries: @home").level, 1);
        QCOMPARE(parseLine("just a note").type, Item::Note);
        QCOMPARE(parseLine("  ").type, Item::Blank);
    }

    void readsTags()
    {
        const Item item = parseLine("- call @done(2026-11-05) @urgent me@example.com");
        QCOMPARE(item.tags.size(), 2);
        QCOMPARE(item.tagValue("done"), QString("2026-11-05"));
        QVERIFY(item.hasTag("urgent"));
        QCOMPARE(item.title, QString("call me@example.com"));
    }

    void findsLinks()
    {
        const auto links = findLinks("- see (https://example.com/a?b=1), or www.example.org. Done");
        QCOMPARE(links.size(), 2);
        QCOMPARE(links[0].url, QString("https://example.com/a?b=1"));
        QCOMPARE(links[1].url, QString("https://www.example.org"));
        QVERIFY(findLinks("mail me@example.com").isEmpty());
    }

    void filters()
    {
        const Item done = parseLine("- a @done(2026-11-05)"), open = parseLine("- b @due(2026-12-01)");
        QVERIFY(Query::parse("@done").matches(done));
        QVERIFY(Query::parse("not @done").matches(open));
        QVERIFY(Query::parse("@due > 2026-11-30").matches(open));
        QVERIFY(!Query::parse("@due < 2026-11-30").matches(open));
        QVERIFY(Query::parse("@done or @due").matches(open));
        QVERIFY(!Query::parse("@done and @due").matches(open));
        QVERIFY(Query::parse("task b").matches(open));
        QVERIFY(Query::parse("project Inbox").matches(parseLine("Inbox:")));
    }

    void subtrees()
    {
        // A list typed without indenting: the project keeps the plain lines under it until the next project.
        QVector<Item> flat = {parseLine("Inbox:"), parseLine("- a"), parseLine("- b"), parseLine("Work:")};
        QVERIFY(isFlatProject(flat, 0));
        QVERIFY(!endsSubtree(flat[0], flat[1], true));
        QVERIFY(endsSubtree(flat[0], flat[3], true));
        // A project with indented contents ends with the indenting; same-level tasks after it are its siblings.
        QVector<Item> nested = {parseLine("\tUI appeal:"), parseLine("\t\t- call"), parseLine("\t\tnotes"), parseLine("\t- sibling task"), parseLine("\tOther:")};
        QVERIFY(!isFlatProject(nested, 0));
        QVERIFY(!endsSubtree(nested[0], nested[1], false));
        QVERIFY(endsSubtree(nested[0], nested[3], false));
        QVERIFY(endsSubtree(nested[0], nested[4], false));
        // Blank lines don't decide it.
        QVector<Item> spaced = {parseLine("Home:"), parseLine(""), parseLine("\t- x")};
        QVERIFY(!isFlatProject(spaced, 0));
    }

    void dateWords()
    {
        const QDate today = QDate::currentDate();
        const auto at = [&](const QDate &d) { return parseLine("- x @due(" + d.toString("yyyy-MM-dd") + ")"); };
        const auto day = [&](const QString &q, const QDate &d) { return Query::parse(q).matches(at(d)); };
        QVERIFY(day("@due = today", today));
        QVERIFY(day("@due = tomorrow", today.addDays(1)));
        QVERIFY(day("@due = today + 1 week", today.addDays(7)));
        QVERIFY(day("@due <= today + 2 days", today.addDays(2)));
        QVERIFY(!day("@due < today", today));
        QVERIFY(day("@due < today", today.addDays(-1)));
        QVERIFY(day("@due > yesterday", today));
        // "next Monday" is always 1 to 7 days ahead and a Monday.
        const QDate next = today.addDays(7 - today.dayOfWeek() + 1);
        QVERIFY(day("@due = next Monday", next));
        QVERIFY(day("@due = monday", today.addDays((1 - today.dayOfWeek() + 7) % 7)));
        QVERIFY(day("@due = next week", today.addDays(1 - today.dayOfWeek() + 7)));
        QVERIFY(day("@due = next month", QDate(today.year(), today.month(), 1).addMonths(1)));
        QVERIFY(day("@due = 2026-06-03", QDate(2026, 6, 3)));
        QVERIFY(day("@due = June 3", QDate(today.year(), 6, 3)));
        QVERIFY(day("@due >= Nov 26 3:15 +1 day", QDate(today.year(), 11, 28)));
        QVERIFY(!day("@due = Nov 26 3:15 +1 day", QDate(today.year(), 11, 26)));
        // Words stay words.
        QVERIFY(Query::parse("@name = bob").matches(parseLine("- x @name(bob)")));
        QVERIFY(Query::parse("@n < 10").matches(parseLine("- x @n(9)")));
    }
};

QTEST_MAIN(TaskPaperTests)
#include "taskpaper_tests.moc"
