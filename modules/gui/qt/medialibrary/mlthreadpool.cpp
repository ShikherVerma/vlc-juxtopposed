/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlthreadpool.hpp"

#include <cassert>
#include <memory>
#include <utility>

#include <QMutexLocker>
#include <QDebug>
#include <QThread>
#include <QOperatingSystemVersion>

void ThreadRunner::traceLocked(const char* operation, quint64 id, const QObject* owner,
                               const RunOnThreadBaseRunner* task)
{
    if (!m_traceEnabled)
        return;
    auto& entry = m_traceOwners[owner];
    const auto line = QStringLiteral("TR ms=%14 seq=%1 op=%2 id=%3 owner=%4 class=%5 runner=%6 task=%7 tid=%8 objectThread=%9 runnerThread=%10 running=%11 owners=%12 pending=%13")
        .arg(++m_traceSequence).arg(QString::fromLatin1(operation)).arg(id)
        .arg(quintptr(owner), 0, 16).arg(QString::fromLatin1(entry.name))
        .arg(quintptr(this), 0, 16).arg(quintptr(task), 0, 16)
        .arg(quintptr(QThread::currentThreadId()), 0, 16)
        .arg(quintptr(entry.thread), 0, 16).arg(quintptr(thread()), 0, 16)
        .arg(m_runningTasks.size()).arg(m_objectTasks.size()).arg(m_pendingTasks.size())
        .arg(QDateTime::currentMSecsSinceEpoch());
    entry.history.append(line);
    if (entry.history.size() > 64)
        entry.history.removeFirst();
    qDebug().noquote() << line;
    // Bound stale-owner storage without evicting an active owner's history.
    if (m_traceOwners.size() > 256)
    {
        for (auto it = m_traceOwners.begin(); it != m_traceOwners.end(); ++it)
            if (it.key() != owner && !m_objectTasks.contains(it.key()))
            {
                m_traceOwners.erase(it);
                break;
            }
    }
}

void ThreadRunner::traceFailureLocked(const QObject* owner)
{
    if (m_traceEnabled)
    {
        // A missing pending entry may have lost its task-to-owner metadata.
        // Dump cached owners too rather than access a possibly dead task.
        if (!owner)
            for (const auto* cachedOwner : m_traceOwners.keys())
                if (cachedOwner)
                    traceFailureLocked(cachedOwner);
        traceLocked("assert-state", 0, owner);
        qDebug().noquote() << "TR STATE owner=" << QString::number(quintptr(owner), 16)
                           << "ownerIds=" << m_objectTasks.values(owner)
                           << "runningIds=" << m_runningTasks.keys();
        for (const auto& line : m_traceOwners.value(owner).history)
            qDebug().noquote() << "TR HISTORY" << line;
    }
}

void ThreadRunner::disconnectTarget(const QObject* owner)
{
    disconnect(owner, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
    traceLocked("disconnect-destroyed", 0, owner);
}

ThreadRunner::ThreadRunner()
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 9, 0))
    // This is currently only applicable on Windows (10 RS3) and Darwin, as of Qt 6.11:
    m_threadPool.setServiceLevel(QThread::QualityOfService::Eco);
#endif

    // Even if we use energy efficient cores, we still need to limit the thread count,
    // because not all processors have energy efficient cores and it is assumed that
    // in that case all cores would be available:

    m_threadPool.setMaxThreadCount(std::clamp(QThread::idealThreadCount() - 1, 1, 4));
}

ThreadRunner::~ThreadRunner()
{
    vlc::qt::shutdownTrace("ThreadRunner-destructor", this);
    if (m_traceEnabled && (!m_objectTasks.empty() || !m_runningTasks.empty() || !m_pendingTasks.empty()))
    {
        QMutexLocker lock(&m_lock);
        // A stale running/pending entry may no longer have an owner index.
        // Include cached owners without dereferencing any destroyed target.
        for (const auto* owner : m_traceOwners.keys())
            traceFailureLocked(owner);
    }
    assert(m_objectTasks.empty());
    assert(m_runningTasks.empty());
    assert(m_pendingTasks.empty());
}

void ThreadRunner::setMaxThreadCount(size_t threadCount)
{
    m_threadPool.setMaxThreadCount(threadCount);
}


void ThreadRunner::start(QRunnable* task, const char* queue)
{
    if (queue == nullptr)
    {
        m_threadPool.start(task);
    }
    else
    {
        QMutexLocker lock(&m_serialTaskLock);
        if (m_serialTasks.contains(queue))
        {
            m_serialTasks[queue].push_back(task);
        }
        else
        {
            m_serialTasks[queue] = QQueue<QRunnable*>();
            m_serialTasks[queue].push_back(task);
            processQueueLocked(queue);
        }
    }
}

bool ThreadRunner::tryTake(QRunnable* task)
{
    auto traceTake = [this, task](const char* operation) {
        if (m_traceEnabled)
        {
            for (auto it = m_traceTasks.cbegin(); it != m_traceTasks.cend(); ++it)
                if (static_cast<const QRunnable*>(it.key()) == task)
                {
                    traceLocked(operation, it.value().first, it.value().second, it.key());
                    break;
                }
        }
    };
    traceTake("take-enter");
    bool ret = m_threadPool.tryTake(task);
    if (ret)
    {
        traceTake("take-pool-success");
        return true;
    }

    {
        QMutexLocker lock(&m_serialTaskLock);
        for (auto queueIt = m_serialTasks.begin(); queueIt != m_serialTasks.end(); ++queueIt)
        {
            auto& queue = queueIt.value();
            auto taskIt = std::find(queue.begin(), queue.end(), task);
            if (taskIt != queue.end())
            {
                queue.erase(taskIt);
                if (queue.empty())
                    m_serialTasks.erase(queueIt);
                traceTake("take-serial-success");
                return true;
            }
        }
    }
    traceTake("take-running");
    return false;
}


QRunnable* ThreadRunner::getNextTaskFromQueue(const QString& queueName)
{
    QMutexLocker lock(&m_serialTaskLock);
    auto& queue = m_serialTasks[queueName];
    if (queue.empty())
    {
        m_serialTasks.remove(queueName);
        return nullptr;
    }
    QRunnable* task = queue.front();
    queue.pop_front();
    return task;
}

void ThreadRunner::processQueueLocked(const QString& queueName)
{
    if (!m_serialTasks.contains(queueName))
        return;

    auto taskId = m_taskId++;
    struct Ctx{};
    auto runnable = new RunOnThreadRunner<Ctx>(taskId, this,
        [this, queueName](Ctx&){
            QRunnable* task = getNextTaskFromQueue(queueName);
            if (!task)
            {
                return;
            }
            // Like QThreadPool, snapshot autoDelete before run(). A task's
            // done signal can already release it on the GUI thread before
            // run() returns; even reading its flags afterwards is a UAF.
            const bool autoDelete = task->autoDelete();
            task->run();
            if (autoDelete)
                delete task;
        },
        [this, queueName](quint64, const Ctx&){
             // Match start()'s lock order. The continuation also allocates a
             // task ID and updates both task maps; the serial-queue lock alone
             // does not protect these against concurrent submissions/cancels.
             QMutexLocker taskLock(&m_lock);
             QMutexLocker queueLock(&m_serialTaskLock);
             processQueueLocked(queueName);
        });
    trackTask(runnable, taskId, this);

    m_threadPool.start(runnable);
}

void ThreadRunner::trackTask(RunOnThreadBaseRunner* task, quint64 id, const QObject* owner)
{
    if (m_traceEnabled && !m_traceOwners.contains(owner))
        m_traceOwners[owner] = {owner->metaObject()->className(), owner->thread(), {}};
    if (m_traceEnabled)
    {
        m_traceTasks.insert(task, {id, owner});
    }
    // Producers may have no event loop. Always attach cleanup to the runner.
    task->moveToThread(thread());
    connect(task, &RunOnThreadBaseRunner::done, this, &ThreadRunner::runOnThreadDone);
    connect(task, &QObject::destroyed, this, [this, task, id, owner] {
        QMutexLocker lock(&m_lock);
        const bool removed = m_pendingTasks.remove(task);
        traceLocked("pending-erase", id, owner, task);
        if (m_traceEnabled && !removed)
            traceFailureLocked(owner);
        if (m_traceEnabled) m_traceTasks.remove(task);
        assert(removed);
        (void)removed;
        if (m_shuttingDown && m_pendingTasks.empty())
            deleteLater();
    }, Qt::DirectConnection);
    m_runningTasks.insert(id, task);
    traceLocked("running-insert", id, owner, task);
    m_objectTasks.insert(owner, id);
    traceLocked("owner-insert", id, owner, task);
    m_pendingTasks.insert(task, std::shared_ptr<RunOnThreadBaseRunner>(task,
        [](RunOnThreadBaseRunner* task) {
            if (task->thread() == QThread::currentThread())
                delete task;
            else
                task->deleteLater();
        }));
    traceLocked("pending-insert", id, owner, task);
}

std::shared_ptr<RunOnThreadBaseRunner> ThreadRunner::releaseTask(RunOnThreadBaseRunner* task)
{
    auto it = m_pendingTasks.find(task);
    if (m_traceEnabled && (it == m_pendingTasks.end() || !it.value()))
        traceFailureLocked(m_traceTasks.value(task).second);
    assert(it != m_pendingTasks.end() && it.value());
    const auto retired = std::exchange(it.value(), nullptr);
    if (m_traceEnabled)
    {
        const auto key = m_traceTasks.value(task);
        traceLocked("pending-release", key.first, key.second, task);
    }
    return retired;
}

void ThreadRunner::removeObjectTask(const QObject* owner, quint64 id)
{
    // Qt's QMultiMap::remove(key, value) scans forward from multimap::find,
    // which need not return the first equivalent key (notably with libc++ on
    // Windows). It can miss this ID and leave an orphan owner entry. The
    // two-argument find uses equal_range; erase that exact iterator instead.
    auto it = m_objectTasks.find(owner, id);
    if (m_traceEnabled && it == m_objectTasks.end())
        traceFailureLocked(owner);
    assert(it != m_objectTasks.end());
    m_objectTasks.erase(it);
}

void ThreadRunner::destroy()
{
    vlc::qt::ShutdownTraceScope trace("ThreadRunner-drain", this);
    QList<std::shared_ptr<RunOnThreadBaseRunner>> retired;
    QMutexLocker locker{&m_lock};
    m_shuttingDown = true;
    traceLocked("drain-enter", 0, nullptr);
    if (m_traceEnabled)
        for (auto it = m_traceTasks.cbegin(); it != m_traceTasks.cend(); ++it)
            traceLocked("drain-pending", it.value().first, it.value().second, it.key());
    //try to cancel as many tasks as possible
    for (auto taskIt = m_objectTasks.begin(); taskIt != m_objectTasks.end(); /**/)
    {
        const QObject* object = taskIt.key();
        quint64 key = taskIt.value();
        auto task = m_runningTasks.value(key, nullptr);
        if (!task) traceFailureLocked(object);
        assert(task);
        if (tryTake(task))
        {
            retired.push_back(releaseTask(task));
            m_runningTasks.remove(key);
            traceLocked("drain-running-erase", key, object, task);
            taskIt = m_objectTasks.erase(taskIt);
            traceLocked("drain-owner-erase", key, object, task);
            if (m_objectTasks.count(object) == 0)
                disconnectTarget(object);
        }
        else
            ++taskIt;
    }

    if (m_pendingTasks.empty())
    {
        deleteLater();
    }
    locker.unlock();
    retired.clear();
}

void ThreadRunner::cancelTask(const QObject* object, quint64 taskId)
{
    if (m_traceEnabled && taskId == 0)
    {
        QMutexLocker lock(&m_lock);
        traceFailureLocked(object);
    }
    assert(taskId != 0);
    QMutexLocker locker{&m_lock};

    // The owner and ID form the cancellation key. Looking up only the ID
    // could remove another object's running task while leaving its owner
    // index behind, causing assert(task) when that object is destroyed.
    traceLocked("cancel-enter", taskId, object);
    if (!m_objectTasks.contains(object, taskId))
        return;
    auto task = m_runningTasks.value(taskId, nullptr);
    if (!task) traceFailureLocked(object);
    assert(task);
    task->cancel();
    bool removed = tryTake(task);
    m_runningTasks.remove(taskId);
    traceLocked("cancel-running-erase", taskId, object, task);
    removeObjectTask(object, taskId);
    traceLocked("cancel-owner-erase", taskId, object, task);
    if (m_objectTasks.count(object) == 0)
        disconnectTarget(object);
    auto retired = removed ? releaseTask(task) : std::shared_ptr<RunOnThreadBaseRunner>{};
    if (m_shuttingDown && m_pendingTasks.empty())
        deleteLater();
    locker.unlock();
    retired.reset();
}

void ThreadRunner::runOnThreadDone(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status)
{
    // Destruction of the last callback reference can reenter the pool.
    // Declare it before the locker so all return paths unlock first.
    std::shared_ptr<RunOnThreadBaseRunner> completion;
    QMutexLocker locker{&m_lock};
    traceLocked("done-enter", target, object, runner);
    completion = releaseTask(runner);
    if (!m_runningTasks.contains(target))
    {
        if (m_shuttingDown && m_pendingTasks.empty())
            deleteLater();
        return;
    }

    if (m_traceEnabled && (m_runningTasks.value(target) != runner || !m_objectTasks.contains(object, target)))
        traceFailureLocked(object);
    assert(m_runningTasks.value(target) == runner);
    assert(m_objectTasks.contains(object, target));
    m_runningTasks.remove(target);
    traceLocked("done-running-erase", target, object, runner);
    removeObjectTask(object, target);
    traceLocked("done-owner-erase", target, object, runner);
    // Keep the destruction barrier connected until foreign-thread delivery
    // has been posted below. Otherwise object could die between removal from
    // the maps and invokeMethod's access to its QObject internals.
    const bool lastTask = m_objectTasks.count(object) == 0;
    if (lastTask && (m_shuttingDown || object->thread() == thread()))
        disconnectTarget(object);

    if (m_shuttingDown)
    {
        if (m_pendingTasks.empty())
            deleteLater();
        return;
    }

    // The queued target event owns cleanup too: destroying the receiver drops
    // the event without calling the UI closure, but must still release the task.
    if (status == ML_TASK_STATUS_SUCCEED)
    {
        if (object->thread() == this->thread())
        {
            locker.unlock();
            completion->runUICallback();
        }
        else
        {
            // Post while the destroyed-handler barrier still protects object.
            // AutoConnection could run inline under m_lock when done is emitted
            // on the target thread, deadlocking a nested submission/cancellation.
            QMetaObject::invokeMethod(const_cast<QObject*>(object),
                [completion] { completion->runUICallback(); }, Qt::QueuedConnection);
            if (lastTask)
                disconnectTarget(object);
        }
    }
    else if (lastTask && object->thread() != thread())
        disconnectTarget(object);
}

void ThreadRunner::runOnThreadTargetDestroyed(QObject * object)
{
    QList<std::shared_ptr<RunOnThreadBaseRunner>> retired;
    QMutexLocker locker{&m_lock};
    traceLocked("destroyed-enter", 0, object);
    if (m_objectTasks.contains(object))
    {
        for (auto taskId : m_objectTasks.values(object))
        {
            auto task = m_runningTasks.value(taskId, nullptr);
            if (!task) traceFailureLocked(object);
            assert(task);
            bool removed = tryTake(task);
            if (removed)
            {
                retired.push_back(releaseTask(task));
            }
            m_runningTasks.remove(taskId);
            traceLocked("destroyed-running-erase", taskId, object, task);
        }
        m_objectTasks.remove(object);
        traceLocked("destroyed-owner-erase", 0, object);
        //no need to disconnect QObject::destroyed, as object is currently being destroyed
    }
    // Taken tasks have no done signal; tasks already running remain pending
    // even though they no longer have a live callback target.
    if (m_shuttingDown && m_pendingTasks.empty())
        deleteLater();
    locker.unlock();
    retired.clear();
}
