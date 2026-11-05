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
};

QTEST_MAIN(TaskPaperTests)
#include "taskpaper_tests.moc"
