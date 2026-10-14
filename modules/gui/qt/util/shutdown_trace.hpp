/*****************************************************************************
 * Opt-in, text-only Qt shutdown diagnostics.
 * Copyright (C) 2026 VLC authors and VideoLAN
 * Licensed under the GNU General Public License, version 2 or later.
 *****************************************************************************/
#ifndef VLC_QT_SHUTDOWN_TRACE_HPP
#define VLC_QT_SHUTDOWN_TRACE_HPP

#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <thread>
#include <atomic>
#include <memory>

namespace vlc::qt {
inline bool shutdownTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("VLC_QT_TRACE_SHUTDOWN") == 1;
    return enabled;
}

inline std::atomic_bool shutdownTraceRequested{false};

// Never inspect the object: many call sites run during partial destruction.
inline void shutdownTrace(const char* operation, const void* object = nullptr,
                          quint64 id = 0, const void* owner = nullptr)
{
    if (shutdownTraceEnabled())
        qDebug().nospace() << "QTSD ms=" << QDateTime::currentMSecsSinceEpoch()
            << " tid=" << Qt::hex << quintptr(QThread::currentThreadId())
            << " op=" << operation << " object=" << quintptr(object)
            << " owner=" << quintptr(owner) << Qt::dec << " id=" << id;
}

// A GUI timer cannot diagnose a blocked GUI thread. This opt-in watchdog only
// prints immutable metadata; it neither touches QObjects nor pumps events.
class ShutdownTraceScope
{
public:
    ShutdownTraceScope(const char* operation, const void* object = nullptr, bool shutdownOnly = false)
        : m_operation(operation), m_object(object), m_shutdownOnly(shutdownOnly)
    {
        if (!shutdownTraceEnabled())
            return;
        m_done = std::make_unique<QWaitCondition>();
        shutdownTrace(m_operation, m_object);
        const auto waitingThread = quintptr(QThread::currentThreadId());
        m_watchdog = std::thread([this, waitingThread] {
            shutdownTrace("watchdog-armed", m_object);
            QMutexLocker lock(&m_mutex);
            while (!m_finished)
            {
                if (m_done->wait(&m_mutex, 2000) || m_finished)
                    continue;
                if (m_shutdownOnly && !shutdownTraceRequested)
                    continue;
                lock.unlock();
                qDebug().nospace() << "QTSD ms=" << QDateTime::currentMSecsSinceEpoch()
                    << " still waiting for " << m_operation << " object=" << Qt::hex
                    << quintptr(m_object) << " blockedTid=" << waitingThread
                    << " watchdogTid=" << quintptr(QThread::currentThreadId())
                    // UCRT treats line buffering as full buffering. Force enough
                    // plain log text through to retain a wait even on forced exit.
                    << " flush-padding=" << QByteArray(8192, '.');
                lock.relock();
            }
        });
    }
    ~ShutdownTraceScope()
    {
        if (!m_watchdog.joinable())
            return;
        {
            QMutexLocker lock(&m_mutex);
            m_finished = true;
        }
        m_done->wakeOne();
        m_watchdog.join();
        qDebug().nospace() << "QTSD ms=" << QDateTime::currentMSecsSinceEpoch()
            << " tid=" << Qt::hex << quintptr(QThread::currentThreadId())
            << " finished=" << m_operation << " object=" << quintptr(m_object);
    }
    ShutdownTraceScope(const ShutdownTraceScope&) = delete;
    ShutdownTraceScope& operator=(const ShutdownTraceScope&) = delete;
private:
    const char* m_operation;
    const void* m_object;
    const bool m_shutdownOnly;
    QMutex m_mutex;
    std::unique_ptr<QWaitCondition> m_done;
    bool m_finished = false;
    std::thread m_watchdog;
};
}
#endif
