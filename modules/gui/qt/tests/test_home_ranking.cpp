/****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
#include <QtTest/QtTest>
#include <limits>
#include "medialibrary/mlhomeranking.hpp"

class TestHomeRanking : public QObject
{
    Q_OBJECT
private slots:
    void progress_data()
    {
        QTest::addColumn<double>("progress");
        QTest::addColumn<qint64>("recent");
        QTest::addColumn<bool>("expected");
        QTest::newRow("never-played") << .3 << qint64(0) << false;
        QTest::newRow("zero") << 0. << qint64(10) << false;
        QTest::newRow("partial") << .3 << qint64(10) << true;
        QTest::newRow("finished") << 1. << qint64(10) << false;
        QTest::newRow("negative") << -.1 << qint64(10) << false;
        QTest::newRow("overrun") << 1.1 << qint64(10) << false;
        QTest::newRow("nan") << std::numeric_limits<double>::quiet_NaN() << qint64(10) << false;
    }
    void progress()
    {
        QFETCH(double, progress);
        QFETCH(qint64, recent);
        QFETCH(bool, expected);
        QCOMPARE(MLHomeRanking::resumable(progress, recent), expected);
    }
    void frequencyAndRecency()
    {
        using namespace MLHomeRanking;
        Rank olderFrequent { 5, 100, QStringLiteral("b"), 2 };
        Rank newer { 1, 200, QStringLiteral("a"), 1 };
        QVERIFY(before(olderFrequent, newer, true));
        QVERIFY(!before(newer, olderFrequent, true));
        QVERIFY(before(newer, olderFrequent, false));
        QVERIFY(!before(olderFrequent, newer, false));
        QVERIFY(before({5, 200, QStringLiteral("b"), 3}, olderFrequent, true));
    }
    void deterministicTies()
    {
        using namespace MLHomeRanking;
        QVERIFY(before({1, 20, QStringLiteral("Alpha"), 9}, {1, 20, QStringLiteral("beta"), 1}, true));
        QVERIFY(before({1, 20, QStringLiteral("ALPHA"), 1}, {1, 20, QStringLiteral("alpha"), 2}, true));
        QVERIFY(!before({1, 20, QStringLiteral("alpha"), 1}, {1, 20, QStringLiteral("alpha"), 1}, true));
    }
};
QTEST_GUILESS_MAIN(TestHomeRanking)
#include "test_home_ranking.moc"
