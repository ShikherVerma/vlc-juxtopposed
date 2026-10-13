/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../maininterface/navigationmodel.hpp"
#include "vlc_stub_modules.hpp"
#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <memory>

class TestVLCNavigationModel : public QObject
{
    Q_OBJECT
    std::unique_ptr<VLCTestingEnv> env;

    static void initialize(NavigationModel& model, bool ml = true)
    {
        model.classBegin();
        model.setHasMedialib(ml);
        model.componentComplete();
    }

    static QStringList route(NavigationModel& model, int row)
    {
        return model.data(model.index(row), NavigationModel::URI).toStringList();
    }

private slots:
    void initTestCase()
    {
        env = std::make_unique<VLCTestingEnv>();
        QVERIFY(env->init());
    }
    void cleanupTestCase() { env.reset(); }

    void authoredRoots()
    {
        NavigationModel model;
        QAbstractItemModelTester tester(&model);
        initialize(model);
        QCOMPARE(model.rowCount(), 7);
        const QStringList titles{"Home", "Videos", "Music", "Network", "Playlists", "Recent", "Add item"};
        const QList<QStringList> routes{{"home"}, {"video"}, {"music"}, {"network"}, {"video", "playlists"}, {"recent"}, {"library-add"}};
        for (int i = 0; i < titles.size(); ++i) {
            QCOMPARE(model.data(model.index(i), NavigationModel::TITLE).toString(), titles[i]);
            QCOMPARE(route(model, i), routes[i]);
            QCOMPARE(model.data(model.index(i), NavigationModel::DEPTH).toInt(), 0);
            QVERIFY(!model.data(model.index(i), NavigationModel::ICON).toString().isEmpty());
        }
    }

    void noMediaLibrary()
    {
        NavigationModel model;
        QAbstractItemModelTester tester(&model);
        initialize(model, false);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(route(model, 0), QStringList{"home"});
        QCOMPARE(route(model, 1), QStringList{"network"});
        QVERIFY(model.setData(model.index(1), true, NavigationModel::EXPANDED));
        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(route(model, 3), (QStringList{"discover", "url"}));
        QCOMPARE(route(model, 4), (QStringList{"discover", "services"}));
    }

    void expansionTransitions()
    {
        NavigationModel model;
        QAbstractItemModelTester tester(&model);
        initialize(model);
        const QList<int> parents{1, 2, 3, 4};
        const QList<int> childCounts{1, 4, 3, 2};
        // Every parent-to-parent switch, repeated expand and collapse preserves
        // the flattening, insertion/removal protocol and authored root order.
        for (int from = 0; from < parents.size(); ++from) {
            for (int to = 0; to < parents.size(); ++to) {
                QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
                QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
                QVERIFY(model.setData(model.index(parents[from]), true, NavigationModel::EXPANDED));
                QCOMPARE(model.rowCount(), 7 + childCounts[from]);
                QCOMPARE(inserted.size(), 1);
                QCOMPARE(inserted[0][1].toInt(), parents[from] + 1);
                QCOMPARE(inserted[0][2].toInt(), parents[from] + childCounts[from]);
                inserted.clear();
                const int target = parents[to] + (parents[to] > parents[from] ? childCounts[from] : 0);
                QVERIFY(model.setData(model.index(target), true, NavigationModel::EXPANDED));
                QCOMPARE(model.rowCount(), 7 + childCounts[to]);
                QCOMPARE(inserted.size(), from == to ? 0 : 1);
                QCOMPARE(removed.size(), from == to ? 0 : 1);
                QVERIFY(model.data(model.index(parents[to]), NavigationModel::EXPANDED).toBool());
                QVERIFY(model.setData(model.index(parents[to]), false, NavigationModel::EXPANDED));
                QCOMPARE(model.rowCount(), 7);
            }
        }
    }

    void childRoutesAndCollapsedMenus()
    {
        NavigationModel model;
        initialize(model);
        const QList<QList<QStringList>> routes{
            {{"video", "all"}},
            {{"music", "tracks"}, {"music", "albums"}, {"music", "artists"}, {"music", "genres"}},
            {{"network"}, {"discover", "url"}, {"discover", "services"}},
            {{"video", "playlists"}, {"music", "playlists"}}
        };
        for (int parent = 1; parent <= 4; ++parent) {
            const auto children = model.data(model.index(parent), NavigationModel::CHILDREN).value<QList<NavigationModelItem>>();
            QCOMPARE(children.size(), routes[parent - 1].size());
            QVERIFY(model.setData(model.index(parent), true, NavigationModel::EXPANDED));
            for (int child = 0; child < children.size(); ++child) {
                QCOMPARE(children[child].uri(), routes[parent - 1][child]);
                QCOMPARE(route(model, parent + child + 1), children[child].uri());
                QCOMPARE(model.data(model.index(parent + child + 1), NavigationModel::DEPTH).toInt(), 1);
            }
            QVERIFY(model.setData(model.index(parent), false, NavigationModel::EXPANDED));
        }
    }

    void invalidRowsAndLeafSwitch()
    {
        NavigationModel model;
        initialize(model);
        QVERIFY(!model.setData(model.index(7), true, NavigationModel::EXPANDED));
        QVERIFY(!model.setData(model.index(-1), false, NavigationModel::EXPANDED));
        QVERIFY(!model.setData(model.index(0), true, NavigationModel::TITLE));
        QVERIFY(model.setData(model.index(2), true, NavigationModel::EXPANDED));
        QCOMPARE(model.rowCount(), 11);
        QVERIFY(model.setData(model.index(0), true, NavigationModel::EXPANDED));
        QCOMPARE(model.rowCount(), 7);
    }
};
// The protocol tester probes QtGui roles as well; initialize their metatypes.
QTEST_MAIN(TestVLCNavigationModel)
#include "test_navigation_model.moc"
