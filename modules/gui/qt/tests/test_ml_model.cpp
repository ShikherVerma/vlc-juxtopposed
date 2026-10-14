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

#include "../medialibrary/mlbasemodel.hpp"
#include "../medialibrary/mlmedia.hpp"
#include "../medialibrary/mlplaylist.hpp"
#include "../medialibrary/mlfolder.hpp"
#include "../medialibrary/mlfoldersmodel.hpp"
#include <QSignalSpy>
#include <QThread>
#include <thread>
#include <atomic>
#include "vlc_stub_modules.hpp"

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QTest>

struct vlc_medialibrary_t;

class MLTestModel : public MLBaseModel
{
    Q_OBJECT

public:
    virtual QVariant itemRoleData(const MLItem *item, int role) const override
    {
        VLC_UNUSED(role);
        if (!item)
            return {};

        return QVariant::fromValue(item->getId());
    }

    std::unique_ptr<MLListCacheLoader> loaderForTest() const
    {
        return createMLLoader();
    }

    void appendRange(int64_t min, int64_t max)
    {
        for (int64_t i = min; i <= max; ++i)
            m_items.emplace_back(i, VLC_ML_PARENT_UNKNOWN);
        emit resetRequested();
    }

    void updateItem(size_t position, int64_t id)
    {
        MLItemId itemId{id, VLC_ML_PARENT_UNKNOWN};
        m_items[position] = itemId;
        updateItemInCache(itemId);
    }


protected:
    std::unique_ptr<MLListCacheLoader> createMLLoader() const override
    {
        return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLTestModel::Loader>(*this, *this));
    }

    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLBaseModel& model, const MLTestModel& parent)
            : MLOp(model)
            , m_mlTestModel(parent)
        {}

        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override
        {
            VLC_UNUSED(ml);
            VLC_UNUSED(queryParams);
            return m_mlTestModel.m_items.size();
        }

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override
        {
            VLC_UNUSED(ml);

            if (m_mlTestModel.m_items.empty())
                return {};

            uint32_t offset = queryParams->i_offset;
            uint32_t count = queryParams->i_nbResults;
            size_t maxIndex = std::min(
                static_cast<size_t>(offset + count),
                m_mlTestModel.m_items.size() - 1);
            std::vector<std::unique_ptr<MLItem>> ret;
            for (size_t i = offset; i <= maxIndex; ++i) {
                ret.emplace_back(std::make_unique<MLItem>(m_mlTestModel.m_items[i]));
            }
            return ret;
        }

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override
        {
            VLC_UNUSED(ml);
            return std::make_unique<MLItem>(itemId);
        }

        const MLTestModel& m_mlTestModel;
    };


    std::vector<MLItemId> m_items;
};

// These callback-only tests never instantiate MainCtx. Supply its type
// identity for the ctx property without linking the entire GUI application.
const QMetaObject MainCtx::staticMetaObject = QObject::staticMetaObject;

// Expose only the native entry point: exercise the production callback and
// real folders-model destructors rather than a parallel mock implementation.
template<class Model>
class TestFoldersModelImpl : public Model
{
public:
    using MLFoldersBaseModel::onMlEvent;

    void eventAtUnregister(std::function<void()> callback)
    {
        this->m_ml_event_handle = MLFoldersBaseModel::EventCallbackPtr(
            reinterpret_cast<vlc_ml_event_callback_t*>(quintptr(1)),
            [callback](vlc_ml_event_callback_t*) { callback(); });
    }
};

using TestFoldersModel = TestFoldersModelImpl<MLFoldersModel>;
using TestBannedFoldersModel = TestFoldersModelImpl<MLBannedFoldersModel>;

static vlc_ml_event_t folderEvent(int type, const char* url)
{
    vlc_ml_event_t event{};
    event.i_type = type;
    switch (type)
    {
    case VLC_ML_EVENT_ENTRY_POINT_ADDED:
        event.entry_point_added.psz_entry_point = url;
        break;
    case VLC_ML_EVENT_ENTRY_POINT_REMOVED:
        event.entry_point_removed.psz_entry_point = url;
        break;
    case VLC_ML_EVENT_ENTRY_POINT_BANNED:
        event.entry_point_banned.psz_entry_point = url;
        break;
    case VLC_ML_EVENT_ENTRY_POINT_UNBANNED:
        event.entry_point_unbanned.psz_entry_point = url;
        break;
    default:
        Q_UNREACHABLE();
    }
    return event;
}

class TestVLCMLModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase() {
        m_env.reset();
    }


    void init() {
        m_medialib = std::make_unique<MediaLib>(m_env->intf, nullptr);
        m_model = std::make_unique<MLTestModel>();
        m_model->classBegin();
        m_model->setMl(m_medialib.get());
    }

    void cleanup() {
        m_model.reset();
        m_medialib.reset();
    }

    void testFolderEventQueuedWithOwnedPayload_data()
    {
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("operation");
        QTest::newRow("add") << int(VLC_ML_EVENT_ENTRY_POINT_ADDED) << int(MLFoldersBaseModel::Add);
        QTest::newRow("remove") << int(VLC_ML_EVENT_ENTRY_POINT_REMOVED) << int(MLFoldersBaseModel::Remove);
        QTest::newRow("ban") << int(VLC_ML_EVENT_ENTRY_POINT_BANNED) << int(MLFoldersBaseModel::Ban);
        QTest::newRow("unban") << int(VLC_ML_EVENT_ENTRY_POINT_UNBANNED) << int(MLFoldersBaseModel::Unban);
    }

    void testFolderEventQueuedWithOwnedPayload()
    {
        QFETCH(int, type);
        QFETCH(int, operation);
        TestFoldersModel folders;
        TestBannedFoldersModel bannedFolders;
        MLFoldersBaseModel& model = operation == MLFoldersBaseModel::Add || operation == MLFoldersBaseModel::Remove
            ? static_cast<MLFoldersBaseModel&>(folders) : static_cast<MLFoldersBaseModel&>(bannedFolders);
        QSignalSpy failed(&model, &MLFoldersBaseModel::operationFailed);
        std::atomic_bool onGuiThread{false};
        connect(&model, &MLFoldersBaseModel::operationFailed, &model, [&] {
            onGuiThread = QThread::currentThread() == model.thread();
        }, Qt::DirectConnection);
        std::thread native([&] {
            QByteArray url("file:///expired-native-event/");
            const auto event = folderEvent(type, url.constData());
            TestFoldersModel::onMlEvent(&model, &event);
            url.fill('x'); // the queued handler must not keep native pointers
        });
        native.join();
        QCOMPARE(failed.count(), 0);
        QCoreApplication::sendPostedEvents(&model, QEvent::MetaCall);
        QCOMPARE(failed.count(), 1);
        QVERIFY(onGuiThread);
        QCOMPARE(failed.at(0).at(0).toInt(), operation);
        QCOMPARE(failed.at(0).at(1).toUrl(), QUrl("file:///expired-native-event/"));
    }

    void testFolderQueuedEventRejectedAfterRebind()
    {
        TestFoldersModel model;
        QSignalSpy failed(&model, &MLFoldersBaseModel::operationFailed);
        vlc_ml_event_t event{};
        event.i_type = VLC_ML_EVENT_ENTRY_POINT_ADDED;
        event.entry_point_added.b_success = false;
        event.entry_point_added.psz_entry_point = "file:///old-binding/";
        TestFoldersModel::onMlEvent(&model, &event);
        model.setCtx(nullptr);
        QCoreApplication::sendPostedEvents(&model, QEvent::MetaCall);
        QCOMPARE(failed.count(), 0);
    }

    void testFolderEventDuringDestruction_data()
    {
        QTest::addColumn<bool>("banned");
        QTest::newRow("folders") << false;
        QTest::newRow("banned-folders") << true;
    }

    void testFolderEventDuringDestruction()
    {
        QFETCH(bool, banned);
        if (banned)
            folderEventDuringDestruction<TestBannedFoldersModel>(VLC_ML_EVENT_ENTRY_POINT_BANNED);
        else
            folderEventDuringDestruction<TestFoldersModel>(VLC_ML_EVENT_ENTRY_POINT_ADDED);
    }

private:
    template<class Model>
    void folderEventDuringDestruction(int type)
    {
        auto model = std::make_unique<Model>();
        auto raw = model.get();
        std::atomic_int signalCount{0};
        connect(raw, &MLFoldersBaseModel::operationFailed, this, [&] { ++signalCount; }, Qt::DirectConnection);
        bool unregistered = false;
        model->eventAtUnregister([&] {
            // Native unregister is a dispatch barrier: an already-entered ML
            // callback may finish while unregister waits. Before the fix this
            // happened only in the base member destructor, after the derived
            // vtable was gone, and failed(event) called a pure virtual method.
            std::thread native([&] {
                const auto event = folderEvent(type, "file:///late-event/");
                TestFoldersModel::onMlEvent(raw, &event);
            });
            native.join();
            // Also exercise a nested event-loop delivery before QObject's
            // own destructor has had a chance to remove queued MetaCalls.
            QCoreApplication::sendPostedEvents(raw, QEvent::MetaCall);
            unregistered = true;
        });
        model.reset();
        QVERIFY(unregistered);
        QCOMPARE(signalCount.load(), 0);
    }

private slots:
    void testMediaLibraryDeletedBeforeQueuedLoad()
    {
        // A cache loader can be retained by an already-posted model event
        // after the GUI's MediaLib has been destroyed (Mac RC quit path).
        m_model->appendRange(1, 20);
        m_model->componentComplete();
        QVERIFY(QTest::qWaitFor([this] { return !m_model->loading(); }));
        auto loader = m_model->loaderForTest();
        bool dispatched = false;
        bool callback = false;
        QMetaObject::invokeMethod(loader.get(), [&] {
            dispatched = true;
            QCOMPARE(loader->countAndLoadTask(0, 10,
                [&](size_t, size_t, std::vector<MLListCacheLoader::ItemType>&) { callback = true; }), size_t(0));
        }, Qt::QueuedConnection);
        m_medialib.reset();
        QVERIFY(!m_model->ml());
        // Reclaim the detached runner before dispatching the surviving loader
        // event. Checking only before DeferredDelete hid the old stale pointer.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::sendPostedEvents(loader.get(), QEvent::MetaCall);
        QVERIFY(dispatched);
        QCOMPARE(loader->countTask([&](size_t, size_t) { callback = true; }), size_t(0));
        QCOMPARE(loader->loadTask(0, 10, [&](size_t, auto&) { callback = true; }), size_t(0));
        QCOMPARE(loader->loadItemsTask(0, {0}, [&](size_t, auto&) { callback = true; }), quint64(0));
        QCOMPARE(loader->loadItemByIdTask({1, VLC_ML_PARENT_UNKNOWN},
            [&](size_t, auto&&) { callback = true; }), size_t(0));
        loader->cancelTask(0);
        loader->cancelTask(42);
        QVERIFY(!callback);
    }

    void testRejectDuringMediaLibraryDestruction()
    {
        auto lib = m_medialib.get();
        bool shuttingDown = false;
        connect(lib, &MediaLib::aboutToDestroy, this, [&] {
            shuttingDown = true;
            struct Empty {};
            auto work = [](vlc_medialibrary_t*, Empty&) { QFAIL("Work submitted during destruction"); };
            auto ui = [](quint64, Empty&) { QFAIL("UI callback during destruction"); };
            QCOMPARE(lib->runOnMLThread<Empty>(this, work, ui), quint64(0));
            auto noContextWork = [](vlc_medialibrary_t*) { QFAIL("Work submitted during destruction"); };
            QCOMPARE(lib->runOnMLThread(this, noContextWork), quint64(0));
            QCOMPARE(lib->runOnMLThread(this, noContextWork, std::function<void()>([] {})), quint64(0));
            QCOMPARE(lib->runOnMLThread(this, noContextWork, std::function<void(quint64)>([](quint64) {})), quint64(0));
            lib->cancelMLTask(this, 0);
            lib->cancelMLTask(this, 42);
        });
        m_medialib.reset();
        QVERIFY(shuttingDown);
        QVERIFY(!m_model->ml());
    }

    /**
     * @brief testModelUpdatedFromCallback
     * test for !6537 scenario
     *
     * - Count&Load task issued (task n°1)
     * - model need reset (thumbnail is updated for instance) => `needReload = true`
     * - Count&Load task (n°1) is resolved
     *    * diff util insert/remove rows
     *    - Callback from model change ask to refer an item beyond the cache
     *    - refer issue a new 'fetch more' request (n°2) as m_countTask is 0
     *    - as `needReload await true`, cache is moved to oldCache, new count&load request
     *       is issued (n° 3)
     * - task n°2 resolve, cache is null, assertion fails
     *
     */
    void testModelRecuseUpdateFromSlot()
    {
        connect(m_model.get(), &BaseModel::countChanged, this, [this](){
            m_model->updateItem(13, 37);
            //model will initially only load 100 elements fetching beyond the loaded range
            // will trigger "fetch more" request
            m_model->data(m_model->index(500), Qt::DisplayRole);
        }, Qt::SingleShotConnection);

        m_model->appendRange(1, 1000);
        m_model->componentComplete();
        QTRY_COMPARE_WITH_TIMEOUT(m_model->getCount(), 1000u, 100);
    }


    void unknownMediaDuration_data()
    {
        QTest::addColumn<qint64>("milliseconds");
        QTest::newRow("unknown") << qint64(-1);
        QTest::newRow("negative") << qint64(-2000);
        QTest::newRow("zero") << qint64(0);
        QTest::newRow("known") << qint64(25000);
    }

    void unknownMediaDuration()
    {
        QFETCH(qint64, milliseconds);
        vlc_ml_media_t raw{};
        raw.i_duration = milliseconds;
        // MLMedia requires a file list, even when empty.
        vlc_ml_file_list_t files{};
        raw.p_files = &files;
        const MLMedia media(&raw);
        QCOMPARE(media.duration().valid(), milliseconds >= 0);
        if (milliseconds < 0)
        {
            QCOMPARE(media.duration().formatHMS(), QString("--:--"));
            QCOMPARE(media.duration().formatShort(), QString("--:--"));
            QCOMPARE(media.duration().formatLong(), QString("--:--"));
        }
        else
            QCOMPARE(media.duration().toMilliseconds(), milliseconds);
        // Signed player delays must not lose their valid negative values.
        QVERIFY(VLCDuration::fromMS(-1).valid());
    }

    void unknownCollectionDuration()
    {
        vlc_ml_playlist_t raw{};
        raw.i_duration = 25000;
        raw.i_nb_duration_unknown = 1;
        QVERIFY(!MLPlaylist(&raw).getDuration().valid());
        raw.i_nb_duration_unknown = 0;
        QCOMPARE(MLPlaylist(&raw).getDuration().toMilliseconds(), 25000);
        raw.i_duration = -1;
        QCOMPARE(MLPlaylist(&raw).getDuration().formatShort(), QString("--:--"));
        raw.i_duration = 0;
        QVERIFY(MLPlaylist(&raw).getDuration().valid());

        // The folder API has no duration field: do not invent a zero total.
        vlc_ml_folder_t folder{};
        QCOMPARE(MLFolder(&folder).getDuration().formatHMS(), QString("--:--"));
    }

    void testGetIndexFromID()
    {
        const int low = 1;
        const int high = 300;
        m_model->appendRange(low, high);
        m_model->componentComplete();

        // let loading complete, getIndexFromID won't work for 'loading' model
        QVERIFY(QTest::qWaitFor([this] () { return !m_model->loading(); }));

        std::optional<int> row = -1;
        auto cb = [&row](std::optional<int> index)
        {
            row = index;
        };

        // bug of MR6926, here model tries to get index for data outside the initial chunk size
        m_model->getIndexFromId2(MLItemId{high, VLC_ML_PARENT_UNKNOWN}, cb);

        // NOTE: CI fails with QTRY_COMPARE_WITH_TIMEOUT
        QVERIFY(QTest::qWaitFor([&row]() { return row != -1; }));
        QCOMPARE(row, high - 1);

        // above we must have loaded all the data, following this we should not need to wait for results
        m_model->getIndexFromId2(MLItemId{low - 1, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, std::nullopt);

        m_model->getIndexFromId2(MLItemId{low, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, low - 1);

        m_model->getIndexFromId2(MLItemId{high + 1, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, std::nullopt);

        const int mid = (low + high) / 2;
        m_model->getIndexFromId2(MLItemId{mid, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, mid - 1);
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<MediaLib> m_medialib;
    std::unique_ptr<MLTestModel> m_model;
};


QTEST_GUILESS_MAIN(TestVLCMLModel)
#include "test_ml_model.moc"
