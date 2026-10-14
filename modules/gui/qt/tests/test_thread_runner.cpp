/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include <QTest>
#include <QSignalSpy>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QEvent>
#include "../medialibrary/mlthreadpool.hpp"
#include "qtestcase.h"
#include <thread>
#include <atomic>
#ifdef Q_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

static const int TASK_TIMEOUT = 300; //MS

class Dummy : public QObject
{
    Q_OBJECT
};

//simple context for testing
struct Ctx {
    int id;
};

struct Barrier {
    Barrier(size_t count)
        : count(count)
    {}

    bool wait(unsigned long timeout = 0) {
        QMutexLocker lock{&mutex};
        --count;
        if (count > 0)
        {
            bool ret = condition.wait(&mutex, timeout);
            if (!ret)
                return false;
        }
        else
            condition.wakeAll();
        return true;
    }
private:
    int count;
    QMutex mutex;
    QWaitCondition condition;
};

class RunnerInThread : public QThread
{
    Q_OBJECT

public:
    void run() override {
        m_runner = new ThreadRunner();
        emit ready();
        exec();
        if (m_runner)
        {
            QSignalSpy deletedSpy(m_runner.get(), &QObject::destroyed);
            m_runner->destroy();
            deletedSpy.wait(TASK_TIMEOUT);
        }
    }

    QPointer<ThreadRunner> m_runner;

signals:
    void ready();
};


class TestThreadRunner : public QObject
{
    Q_OBJECT

private:

    ThreadRunner* getRunner()
    {
        QFETCH_GLOBAL(bool, threaded);
        if (threaded)
            return m_runnerThread->m_runner.get();
        else
            return m_runner.get();
    }

    enum TaskStatus {
        PENDING,
        FAILED,
        SUCCESS
    };

#define CHECK_TASK_TIMEOUT(task) QCOMPARE(QTest::qWaitFor([&task](){ return task != PENDING;}, TASK_TIMEOUT), false);
#define CHECK_TASK_COMPLETE(task) QCOMPARE(QTest::qWaitFor([&task](){ return task != PENDING;}, TASK_TIMEOUT), true); QCOMPARE(task, SUCCESS);

private slots:
    void init() {
        m_runner = new ThreadRunner();
        m_runnerThread = std::make_unique<RunnerInThread>();
        QSignalSpy startSpy(m_runnerThread.get(), &RunnerInThread::ready);
        m_runnerThread->start();
        startSpy.wait(TASK_TIMEOUT);
    }

    void cleanup() {
        if (m_runner) {
            QSignalSpy deletedSpy(m_runner.get(), &QObject::destroyed);
            m_runner->destroy();
            deletedSpy.wait();
        }
        m_runnerThread->quit();
        m_runnerThread->wait();
    }

    void initTestCase_data()
    {
        QTest::addColumn<bool>("threaded");
        QTest::newRow("same thread") << false;
        QTest::newRow("different thread") << true;
    }

    void testSimpleTask()
    {
        ThreadRunner* runner = getRunner();
        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();
        TaskStatus ret = PENDING;
        runner->runOnThread<Ctx>(dummy.get(), [](Ctx& ctx){
            ctx.id = 42;
        }, [&ret](size_t, const Ctx& ctx){
            if (ctx.id == 42)
                ret = SUCCESS;
            else
                ret = FAILED;
        });
        CHECK_TASK_COMPLETE(ret);
    }

    void testParallelTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(3);

        TaskStatus ret1 = PENDING;
        TaskStatus ret2 = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T1
            [&barrierPre, &ret1](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret1 = FAILED;
                    return;
                }
            },
            [&ret1](size_t, const Ctx&){
                if (ret1 != FAILED)
                    ret1 = SUCCESS;
            }
        );

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T2
            [&barrierPre, &ret2](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret2 = FAILED;
                    return;
                }
            },
            [&ret2](size_t, const Ctx&){
                if (ret2 != FAILED)
                    ret2 = SUCCESS;
            }
        );

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(ret1);
        CHECK_TASK_COMPLETE(ret2);
    }


    void testParallelTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();
        QMutex mutex;

        Barrier barrierPre(3);

        TaskStatus ret1 = PENDING;
        TaskStatus ret2 = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T1
            [&mutex, &barrierPre, &ret1](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret1 = FAILED;
                    return;
                }
            },
            [&ret1](size_t, const Ctx&){
                if (ret1 != FAILED)
                    ret1 = SUCCESS;
            },
            "queue1"
        );

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T2
            [&barrierPre, &ret2](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret2 = FAILED;
                    return;
                }
            },
            [&ret2](size_t, const Ctx&){
                if (ret2 != FAILED)
                    ret2 = SUCCESS;
            },
            "queue2" //not the same queue
        );

        //Both tasks should be running at the same time
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(ret1);
        CHECK_TASK_COMPLETE(ret2);
    }

    void testQueuedTask()
    {
        ThreadRunner* runner = getRunner();
        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        //theses tasks should execute sequentially
        int ret = 0;
        runner->runOnThread<Ctx>(dummy.get(),
            [](Ctx& ){
                //N/A
            }, [&ret](size_t, const Ctx&){
                if (ret != 0)
                    ret = -1;
                else
                    ret = 1;
             }, "queue");

        runner->runOnThread<Ctx>(dummy.get(),
            [](Ctx& ){
                //N/A
            },
            [&ret](size_t, const Ctx& ){
                if (ret != 1)
                    ret = -2;
                else
                    ret = 2;
            }, "queue");

        runner->runOnThread<Ctx>(dummy.get(),
             [](Ctx& ){
                 //N/A
             },
             [&ret](size_t, const Ctx& ){
                 if (ret != 2)
                     ret = -3;
                 else
                     ret = 3;
             }, "queue");

        QTRY_COMPARE_WITH_TIMEOUT(ret, 3, TASK_TIMEOUT);
    }


    void testTargetDeletedDuringTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus ret = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }
            },
            [&ret](size_t, const Ctx&){
                //this should not be executed
                ret = FAILED;
            });

        {
            //wait for the task to be started
            QVERIFY(barrierPre.wait(TASK_TIMEOUT));
            dummy.reset();
            QVERIFY(barrierPost.wait(TASK_TIMEOUT));
        }
        CHECK_TASK_TIMEOUT(ret);
    }

    void testTargetDeletedDuringTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus ret = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }
            },
            [&ret](size_t, const Ctx&){
                //this should not be executed
                ret = FAILED;
            }, "queued");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(ret);
    }

    void testTargetDeletedBeforeStart()
    {
        ThreadRunner* runner = getRunner();


        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task1UIret = PENDING;
        TaskStatus task2ret = PENDING;

        //there is only 1 worker thread so tasks will be executed consecutivelly
        runner->setMaxThreadCount(1);

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
                task1ret = SUCCESS;
            },
            [&task1UIret](size_t, const Ctx&){
                //target is deleted this should not be executed
                task1UIret = FAILED;
            });

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            });

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        //task1 thread callback execute
        CHECK_TASK_COMPLETE(task1ret);
        //this should timeout
        CHECK_TASK_TIMEOUT(task2ret);
        QCOMPARE(task1UIret, PENDING);
    }

    void testTargetDeletedBeforeStartQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task1UIret = PENDING;
        TaskStatus task2ret = PENDING;

        //task are on the same queue so they will execute consecutively
        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
                task1ret = SUCCESS;
            },
            [&task1UIret](size_t, const Ctx&){
                //target is deleted this should not be executed
                task1UIret = FAILED;
            },
            "samequeue");

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            },
            "samequeue");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        //task1 thread callback execute
        CHECK_TASK_COMPLETE(task1ret);
        //this should timeout
        CHECK_TASK_TIMEOUT(task2ret);
        QCOMPARE(task1UIret, PENDING);
    }


    void testTaskCanceledBeforeStart()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task2ret = PENDING;

        //task are on the same queue so they will execute consecutively
        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
            },
            [&task1ret](size_t, const Ctx&){
                if (task1ret != FAILED)
                    task1ret = SUCCESS;
            },
            "samequeue");

        quint64 task2 = runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            },
            "samequeue");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), task2);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(task1ret);
        CHECK_TASK_TIMEOUT(task2ret);
    }

    void testTaskCanceledDuringExecution()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        quint64 taskId = runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            });

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), taskId);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testTaskCanceledDuringExecutionQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        quint64 taskId = runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            }, "queued");

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), taskId);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testRunnerDestroyedDuringExecution()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //runner is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            });

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        //destroy runner in its thread
        if (runner->thread() == this->thread())
            runner->destroy();
        else
            QMetaObject::invokeMethod(runner, &ThreadRunner::destroy, Qt::BlockingQueuedConnection);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testRunnerDestroyedDuringExecutionQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //runner is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            }, "queue");

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        //destroy runner in its thread
        if (runner->thread() == this->thread())
            runner->destroy();
        else
            QMetaObject::invokeMethod(runner, &ThreadRunner::destroy, Qt::BlockingQueuedConnection);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }


    void testConcurrentSerialContinuation()
    {
        ThreadRunner* runner = getRunner();
        auto dummy = std::make_unique<Dummy>();
        std::atomic<unsigned> completed{0};
        constexpr unsigned count = 2000;
        // A serialized queue continuation modifies the task maps on the
        // runner's thread while another thread submits tasks. Both paths
        // must use the same lock, including allocation of unique task IDs.
        std::thread producer([&] {
            for (unsigned i = 0; i < count; ++i)
            {
                runner->runOnThread<Ctx>(dummy.get(), [](Ctx&) {},
                    [&completed](quint64, Ctx&) { ++completed; }, "concurrent");
                if ((i % 8) == 0)
                    std::this_thread::yield();
            }
        });
        QElapsedTimer timeout;
        timeout.start();
        while (completed != count && timeout.elapsed() < 10000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        producer.join();
        QCOMPARE(completed.load(), count);
    }

    void testCancelWrongTargetThenDestroy_data()
    {
        QTest::addColumn<bool>("queued");
        QTest::newRow("parallel") << false;
        QTest::newRow("serial") << true;
    }

    void testCancelWrongTargetThenDestroy()
    {
        QFETCH(bool, queued);
        auto runner = getRunner();
        auto owner = std::make_unique<Dummy>();
        Dummy other;
        auto started = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        bool called = false;
        const auto id = runner->runOnThread<Ctx>(owner.get(),
            [started, release](Ctx&) {
                started->release();
                release->tryAcquire(1, 5000);
            }, [&called](quint64, Ctx&) { called = true; },
            queued ? "wrong-target" : nullptr);
        QVERIFY(started->tryAcquire(1, 5000));
        // A task ID is not sufficient to cancel another object's work. The
        // old code removed only the running-task entry, orphaning the owner
        // index; destroying the actual owner then hit assert(task).
        runner->cancelTask(&other, id);
        owner.reset();
        release->release();
        QTest::qWait(50);
        QVERIFY(!called);
    }

    void testOutOfOrderSameOwnerCompletion()
    {
        // Windows trace: the newer of two same-owner tasks completes first;
        // Qt remove(key,value) missed its entry, then owner destruction asserted.
        // Gates, not scheduler sleeps, select that sequence. Linux's STL does
        // not exhibit the Qt bug, but exercises the same map/connection lifetime.
        auto runner = getRunner();
        runner->setMaxThreadCount(2);
        auto owner = std::make_unique<Dummy>();
        auto firstStarted = std::make_shared<QSemaphore>();
        auto firstRelease = std::make_shared<QSemaphore>();
        auto secondRelease = std::make_shared<QSemaphore>();
        bool firstDone = false, secondDone = false;
        const auto first = runner->runOnThread<Ctx>(owner.get(),
            [firstStarted, firstRelease](Ctx&) {
                firstStarted->release();
                firstRelease->tryAcquire(1, 5000);
            }, [&firstDone](quint64, Ctx&) { firstDone = true; });
        QVERIFY(firstStarted->tryAcquire(1, 5000));
        const auto second = runner->runOnThread<Ctx>(owner.get(),
            [secondRelease](Ctx&) { secondRelease->tryAcquire(1, 5000); },
            [&secondDone](quint64, Ctx&) { secondDone = true; });
        secondRelease->release();
        QTRY_VERIFY_WITH_TIMEOUT(secondDone, 5000);
        {
            QMutexLocker lock(&runner->m_lock);
            QVERIFY(runner->m_runningTasks.contains(first));
            QVERIFY(!runner->m_runningTasks.contains(second));
            QCOMPARE(runner->m_objectTasks.values(owner.get()), QList<quint64>{first});
        }
        firstRelease->release();
        QTRY_VERIFY_WITH_TIMEOUT(firstDone, 5000);
        {
            QMutexLocker lock(&runner->m_lock);
            QVERIFY(!runner->m_objectTasks.contains(owner.get()));
        }
        owner.reset(); // keep the production assert(task) in this path
    }

    void testSameOwnerCancelRemovesExactId()
    {
        auto runner = getRunner();
        auto owner = std::make_unique<Dummy>();
        auto started = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        runner->setMaxThreadCount(1);
        const auto first = runner->runOnThread<Ctx>(owner.get(),
            [started, release](Ctx&) {
                started->release();
                release->tryAcquire(1, 5000);
            }, [](quint64, Ctx&) {});
        QVERIFY(started->tryAcquire(1, 5000));
        const auto second = runner->runOnThread<Ctx>(owner.get(), [](Ctx&) {},
            [](quint64, Ctx&) { QFAIL("Canceled UI callback executed"); });
        runner->cancelTask(owner.get(), second);
        {
            QMutexLocker lock(&runner->m_lock);
            QCOMPARE(runner->m_objectTasks.values(owner.get()), QList<quint64>{first});
        }
        owner.reset();
        release->release();
    }

    void testFinishedBeforeTargetDestroyed()
    {
        // Intercept the queued done event before its slot executes: the ML
        // function has finished and completion is posted, but UI work has
        // not run. No scheduler timing or sleeps choose this boundary.
        auto runner = m_runner.get();
        auto owner = std::make_unique<Dummy>();
        struct DestroyOnDone : QObject {
            std::function<void()> destroyTarget;
            bool eventFilter(QObject*, QEvent* event) override
            {
                if (event->type() == QEvent::MetaCall)
                    destroyTarget();
                return false;
            }
        } filter;
        filter.destroyTarget = [&owner] { owner.reset(); };
        runner->installEventFilter(&filter);
        bool called = false;
        runner->runOnThread<Ctx>(owner.get(), [](Ctx&) {},
            [&called](quint64, Ctx&) { called = true; });
        QTRY_VERIFY_WITH_TIMEOUT(!owner, 5000);
        QVERIFY(!called);
    }

    void testCancelThenTargetDestroyed()
    {
        auto runner = getRunner();
        auto owner = std::make_unique<Dummy>();
        auto started = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        bool called = false;
        const auto id = runner->runOnThread<Ctx>(owner.get(),
            [started, release](Ctx&) {
                started->release();
                release->tryAcquire(1, 5000);
            }, [&called](quint64, Ctx&) { called = true; });
        QVERIFY(started->tryAcquire(1, 5000));
        runner->cancelTask(owner.get(), id);
        owner.reset();
        release->release();
        QTest::qWait(50);
        QVERIFY(!called);
    }

    void testTargetDestroyedDuringDrain_data()
    {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("destroy-target") << false;
        QTest::newRow("cancel-then-destroy-target") << true;
    }

    void testTargetDestroyedDuringDrain()
    {
        QFETCH(bool, cancel);
        // Use the main-thread runner so DeferredDelete cannot execute until
        // the blocked worker has been released below.
        auto runner = m_runner.get();
        auto owner = std::make_unique<Dummy>();
        auto started = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        bool called = false;
        const auto id = runner->runOnThread<Ctx>(owner.get(),
            [started, release](Ctx&) {
                started->release();
                release->tryAcquire(1, 5000);
            }, [&called](quint64, Ctx&) { called = true; });
        QVERIFY(started->tryAcquire(1, 5000));
        runner->destroy();
        if (cancel)
            runner->cancelTask(owner.get(), id);
        owner.reset();
        const std::weak_ptr<QSemaphore> workerContext = release;
        release->release();
        release.reset();
        QTRY_VERIFY_WITH_TIMEOUT(m_runner.isNull(), 5000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(workerContext.expired());
        QVERIFY(!called);
    }

    void testCompletionCannotDeleteEmittingTask()
    {
        auto runner = m_runner.get();
        Dummy owner;
        auto start = std::make_shared<QSemaphore>();
        auto emitting = std::make_shared<QSemaphore>();
        auto resume = std::make_shared<QSemaphore>();
        const auto id = runner->runOnThread<Ctx>(&owner,
            [start](Ctx&) { start->tryAcquire(1, 5000); }, [](quint64, Ctx&) {});
        QPointer<RunOnThreadBaseRunner> task;
        {
            QMutexLocker lock(&runner->m_lock);
            task = runner->m_runningTasks.value(id);
            connect(task, &RunOnThreadBaseRunner::done, task, [emitting, resume] {
                emitting->release();
                resume->tryAcquire(1, 5000);
            }, Qt::DirectConnection);
        }
        start->release();
        QVERIFY(emitting->tryAcquire(1, 5000));
        // The queued done handler and DeferredDelete both run while the
        // worker is deterministically still inside QObject::activate(done).
        QCoreApplication::sendPostedEvents(runner, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(task, QEvent::DeferredDelete);
        const bool aliveDuringEmission = !task.isNull();
        resume->release();
        QVERIFY(aliveDuringEmission);
        QTRY_VERIFY_WITH_TIMEOUT(task.isNull(), 5000);
    }

    void testSelfDeletingSerialTask()
    {
#ifdef Q_OS_UNIX
        // A non-auto-delete runnable may release itself in run(). Unmap its
        // storage immediately, so a wrapper's post-run flag read fails every
        // time rather than depending on allocator reuse/timing.
        struct SelfDeletingTask : QRunnable {
            std::atomic_int* destroyed;
            explicit SelfDeletingTask(std::atomic_int* count) : destroyed(count) { setAutoDelete(false); }
            ~SelfDeletingTask() override { ++*destroyed; }
            void run() override { delete this; }
            static void* operator new(size_t) {
                void* p = mmap(nullptr, sysconf(_SC_PAGESIZE), PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (p == MAP_FAILED)
                    throw std::bad_alloc();
                return p;
            }
            static void operator delete(void* p) { munmap(p, sysconf(_SC_PAGESIZE)); }
        };
        auto runner = getRunner();
        std::atomic_int destroyed{0};
        {
            QMutexLocker lock(&runner->m_lock);
            runner->start(new SelfDeletingTask(&destroyed), "self-deleting");
        }
        QTRY_COMPARE_WITH_TIMEOUT(destroyed.load(), 1, 5000);
#else
        QSKIP("Deterministic unmapped-storage regression is Unix-only");
#endif
    }

    void testProducerThreadDoesNotOwnCleanup()
    {
        auto runner = getRunner();
        Dummy owner;
        auto context = std::make_shared<int>(42);
        const std::weak_ptr<int> weak = context;
        bool called = false;
        std::thread producer([&] {
            runner->runOnThread<Ctx>(&owner, [](Ctx&) {},
                [context, &called](quint64, Ctx&) { called = true; });
        });
        producer.join(); // no producer event loop exists to process deleteLater
        context.reset();
        QTRY_VERIFY_WITH_TIMEOUT(called, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(weak.expired(), 5000);
    }

    void testDiscardedForeignCallbackReleasesContext()
    {
        auto runner = m_runnerThread->m_runner.get();
        auto owner = std::make_unique<Dummy>();
        auto context = std::make_shared<int>(42);
        const std::weak_ptr<int> weak = context;
        QSemaphore delivered;
        struct DoneFilter : QObject {
            QSemaphore* delivered;
            bool eventFilter(QObject* receiver, QEvent* event) override
            {
                if (event->type() == QEvent::MetaCall)
                    {
                    receiver->removeEventFilter(this);
                    auto semaphore = delivered;
                    QMetaObject::invokeMethod(receiver, [semaphore] { semaphore->release(); }, Qt::QueuedConnection);
                }
                return false;
            }
        };
        // Install in the runner thread; remove before the marker MetaCall so
        // it only observes the worker's done event, not its own marker.
        auto filter = new DoneFilter;
        filter->delivered = &delivered;
        filter->moveToThread(runner->thread());
        QMetaObject::invokeMethod(runner, [=] { runner->installEventFilter(filter); }, Qt::BlockingQueuedConnection);
        bool called = false;
        runner->runOnThread<Ctx>(owner.get(), [](Ctx&) {},
            [context, &called](quint64, Ctx&) { called = true; });
        context.reset();
        QVERIFY(delivered.tryAcquire(1, 5000));
        QMetaObject::invokeMethod(runner, [=] { runner->removeEventFilter(filter); delete filter; }, Qt::BlockingQueuedConnection);
        // No GUI event processing has occurred: completion is queued to owner.
        QVERIFY(!called);
        owner.reset();
        QTRY_VERIFY_WITH_TIMEOUT(weak.expired(), 5000);
        QVERIFY(!called);
    }

    void testCancelReentrantClosureDestructor()
    {
        auto runner = getRunner();
        auto owner = std::make_unique<Dummy>();
        auto started = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        runner->setMaxThreadCount(1);
        runner->runOnThread<Ctx>(owner.get(), [started, release](Ctx&) {
            started->release();
            release->tryAcquire(1, 5000);
        }, [](quint64, Ctx&) {});
        QVERIFY(started->tryAcquire(1, 5000));
        const auto other = runner->runOnThread<Ctx>(owner.get(), [](Ctx&) {}, [](quint64, Ctx&) {});
        // Releasing a captured resource may cancel another task. No arbitrary
        // closure/QObject destructor may execute with the bookkeeping lock held.
        auto resource = std::shared_ptr<int>(new int(0), [runner, target = owner.get(), other](int* p) {
            runner->cancelTask(target, other);
            delete p;
        });
        const auto first = runner->runOnThread<Ctx>(owner.get(), [](Ctx&) {},
            [resource](quint64, Ctx&) {});
        resource.reset();
        runner->cancelTask(owner.get(), first);
        release->release();
    }

    void testNestedTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [](Ctx& ctx) {
                //run on worker thread
                ctx.id = 1;
            },
            [runner, &taskRet, dummy = dummy.get()](size_t, const Ctx& ctx){
                if (ctx.id != 1)
                {
                    taskRet = FAILED;
                    return;
                }
                runner->runOnThread<Ctx>(
                    dummy,
                    [](Ctx& ctx) {
                        ctx.id = 2;
                    },
                    [&taskRet](size_t, const Ctx& ctx){
                        taskRet = (ctx.id == 2 ? SUCCESS : FAILED);
                    });
            }
            );
        CHECK_TASK_COMPLETE(taskRet);
    }

    void testNestedTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [](Ctx& ctx) {
                //run on worker thread
                ctx.id = 1;
            },
            [runner, &taskRet, dummy = dummy.get()](size_t, const Ctx& ctx){
                if (ctx.id != 1)
                {
                    taskRet = FAILED;
                    return;
                }
                runner->runOnThread<Ctx>(
                    dummy,
                    [](Ctx& ctx) {
                        ctx.id = 2;
                    },
                    [&taskRet](size_t, const Ctx& ctx){
                        taskRet = (ctx.id == 2 ? SUCCESS : FAILED);
                    },
                    "queue"
                );
            },
            "queue"
            );
        CHECK_TASK_COMPLETE(taskRet);
    }

private:
    //A runner living in the main thread
    QPointer<ThreadRunner> m_runner;

    //A runner living in a different thread
    std::unique_ptr<RunnerInThread> m_runnerThread;
};

QTEST_GUILESS_MAIN(TestThreadRunner)
#include "test_thread_runner.moc"
