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

#ifndef MLTHREADPOOL_HPP
#define MLTHREADPOOL_HPP

#include <QObject>
#include <QRunnable>
#include <QQueue>
#include <QMap>
#include <QHash>
#include <memory>
#include <atomic>
#include <functional>
#include <QThreadPool>
#include <QMutex>
#include <QStringList>
#include "util/shutdown_trace.hpp"

class RunOnThreadBaseRunner;

class ThreadRunner : public QObject
{
    Q_OBJECT

public:
    enum MLTaskStatus {
        ML_TASK_STATUS_SUCCEED,
        ML_TASK_STATUS_CANCELED
    };

    ThreadRunner();
    ~ThreadRunner();

    void setMaxThreadCount(size_t threadCount);

    void destroy();
    void cancelTask(const QObject* object, quint64 taskId);

    template<typename Ctx>
    quint64 runOnThread(const QObject* obj,
                          std::function<void (Ctx&)> mlFun,
                          std::function<void (quint64 taskId, Ctx&)> uiFun,
                          const char* queue = nullptr);

private slots:
    void runOnThreadDone(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status);
    void runOnThreadTargetDestroyed(QObject * object);


private:
    /**
     * @brief start enqueue a QRunnable to be executed on the threadpool
     * @param task is the task to enqueue
     * @param queue, the name of the queue, all task with the same queue name will be
     * ran sequentially, if queue is null, task will be run without additional specific
     * constraint (like on QThreadPool)
     */
    void start(QRunnable* task, const char* queue = nullptr);

    /**
     * @brief tryTake atempt to the specified task from the queue if the task has not started
     * @return true if the task has been removed from the queue and was not started yet
     */
    bool tryTake(QRunnable* task);

    QRunnable* getNextTaskFromQueue(const QString& queueName);
    void processQueueLocked(const QString& queueName);
    // Caller holds m_lock. Pending entries outlive callback ownership and
    // remain until the runnable (including its worker/closure state) is gone.
    void trackTask(RunOnThreadBaseRunner* task, quint64 id, const QObject* owner);
    std::shared_ptr<RunOnThreadBaseRunner> releaseTask(RunOnThreadBaseRunner* task);
    void removeObjectTask(const QObject* owner, quint64 id);

private:
    QThreadPool m_threadPool;

    friend class MLThreadPoolSerialTask;
    friend class TestThreadRunner;
    QMutex m_serialTaskLock;
    QMap<QString, QQueue<QRunnable*>> m_serialTasks;

    bool m_shuttingDown = false;
    quint64 m_taskId = 1;
    QMap<quint64, RunOnThreadBaseRunner*> m_runningTasks;
    QMultiMap<const QObject*, quint64> m_objectTasks;
    // Null values have transferred ownership to execution/UI delivery. Keep
    // their keys until QObject destruction so drain includes closure cleanup.
    QHash<RunOnThreadBaseRunner*, std::shared_ptr<RunOnThreadBaseRunner>> m_pendingTasks;
    QMutex m_lock;

    // Opt-in diagnostics only; all history access is serialized by m_lock.
    const bool m_traceEnabled = qEnvironmentVariableIntValue("VLC_QT_TRACE_THREADRUNNER") == 1
        || vlc::qt::shutdownTraceEnabled();
    struct OwnerTrace { QByteArray name; const QThread* thread = nullptr; QStringList history; };
    QMap<const QObject*, OwnerTrace> m_traceOwners;
    QHash<const RunOnThreadBaseRunner*, QPair<quint64, const QObject*>> m_traceTasks;
    quint64 m_traceSequence = 0;
    void traceLocked(const char* operation, quint64 id, const QObject* owner,
                     const RunOnThreadBaseRunner* task = nullptr);
    void traceFailureLocked(const QObject* owner);
    void disconnectTarget(const QObject* owner);

};

class RunOnThreadBaseRunner : public QObject, public QRunnable, public std::enable_shared_from_this<RunOnThreadBaseRunner>
{
    Q_OBJECT
public:
    virtual ~RunOnThreadBaseRunner() = default;
    virtual void runUICallback() = 0;
    virtual void cancel() = 0;

signals:
    void done(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status);
};

template<typename Ctx>
class RunOnThreadRunner : public RunOnThreadBaseRunner {
public:
    RunOnThreadRunner(
        quint64 taskId,
        const QObject* obj,
        std::function<void (Ctx&)> mlFun,
        std::function<void (quint64, Ctx&)> uiFun
        )
        : RunOnThreadBaseRunner()
        , m_taskId(taskId)
        , m_obj(obj)
        , m_mlFun(mlFun)
        , m_uiFun(uiFun)
    {
        setAutoDelete(false);
    }

    void run() override
    {
        // GUI completion may be delivered before emit done has unwound.
        // Keep the signal sender (and its closures) alive through emission.
        auto execution = shared_from_this();
        vlc::qt::shutdownTrace("worker-enter", this, m_taskId, m_obj);
        if (m_canceled)
        {
            emit done(this, m_taskId, m_obj, ThreadRunner::ML_TASK_STATUS_CANCELED);
            vlc::qt::shutdownTrace("worker-canceled", this, m_taskId, m_obj);
            return;
        }
        m_mlFun(m_ctx);
        vlc::qt::shutdownTrace("worker-function-return", this, m_taskId, m_obj);
        emit done(this, m_taskId, m_obj, ThreadRunner::ML_TASK_STATUS_SUCCEED);
        vlc::qt::shutdownTrace("worker-exit", this, m_taskId, m_obj);
    }

    //called from UI thread
    void runUICallback() override
    {
        vlc::qt::shutdownTrace("ui-callback-enter", this, m_taskId, m_obj);
        m_uiFun(m_taskId, m_ctx);
        vlc::qt::shutdownTrace("ui-callback-exit", this, m_taskId, m_obj);
    }

    void cancel() override
    {
        m_canceled = true;
    }
private:
    std::atomic_bool m_canceled {false};
    quint64 m_taskId;
    Ctx m_ctx; //default constructed
    const QObject* m_obj = nullptr;
    const std::function<void (Ctx&)> m_mlFun;
    const std::function<void (quint64, Ctx&)> m_uiFun;
};

template<typename Ctx>
quint64 ThreadRunner::runOnThread(const QObject* obj,
                                      std::function<void (Ctx&)> mlFun,
                                      std::function<void (quint64 taskId, Ctx&)> uiFun,
                                      const char* queue)
{
    QMutexLocker locker{&m_lock};

    if (m_shuttingDown)
        return 0;

    auto taskId = m_taskId++;
    auto runnable = new RunOnThreadRunner<Ctx>(taskId, obj, mlFun, uiFun);
    if (m_traceEnabled)
        m_traceOwners[obj] = {obj->metaObject()->className(), obj->thread(), m_traceOwners.value(obj).history};
    connect(obj, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed, Qt::DirectConnection);
    traceLocked("connect-destroyed", taskId, obj, runnable);
    trackTask(runnable, taskId, obj);
    start(runnable, queue);
    return taskId;
}

#endif // MLTHREADPOOL_HPP
