/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "compositor_platform.hpp"

#include <QApplication>
#include <QQuickView>
#include <QOperatingSystemVersion>

#include "maininterface/interface_window_handler.hpp"

#include <vlc_window.h>

#ifdef __APPLE__
#include <objc/runtime.h>
#ifdef Q_OS_MACOS
#include <objc/message.h>
#include <CoreGraphics/CGGeometry.h>
#endif
#endif

using namespace vlc;


CompositorPlatform::CompositorPlatform(qt_intf_t *p_intf, QObject *parent)
    : CompositorVideo(p_intf, parent)
{

}

bool CompositorPlatform::init()
{
    // TODO: For now only qwindows and qdirect2d
    //       running on Windows 8+, and cocoa
    //       platforms are supported.

    const QString& platformName = qApp->platformName();

#ifdef _WIN32
    if (QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows8)
    {
        if (platformName == QLatin1String("windows") || platformName == QLatin1String("direct2d"))
        {
            // Video embedding with RHI often does not work, VirtualBox drivers may work, but VMWare
            // and Nvidia drivers (non-VM) don't. So we only allow this in non-RHI modes at the
            // moment. Note that using software rasterizer (such as D3D Warp), that can be requested
            // with `QSG_RHI_PREFER_SOFTWARE_RENDERER`, also does not work.
            if (qEnvironmentVariable("QT_QUICK_BACKEND", QStringLiteral("rhi")) == QLatin1String("rhi"))
                return false;

            return true; // Practically "software" and "openvg" modes (non-RHI).
        }
    }
#endif

#ifdef __APPLE__
    if (platformName == QLatin1String("cocoa") || platformName == QLatin1String("ios"))
        return true;
#endif

    return false;
}

bool CompositorPlatform::makeMainInterface(MainCtx *mainCtx, std::function<void (QQuickWindow *)> aboutToShowQuickWindowCallback)
{
    m_mainCtx = mainCtx;

    m_rootWindow = std::make_unique<QWindow>();
#ifdef Q_OS_MACOS
    if (qEnvironmentVariableIsSet("VLC_QT_NO_ACTIVATE"))
        m_rootWindow->setFlag(Qt::WindowDoesNotAcceptFocus);
#endif

    m_videoWindow = new QWindow(m_rootWindow.get());

    m_quickWindow = new QQuickView(m_rootWindow.get());
    m_quickWindow->setResizeMode(QQuickView::SizeRootObjectToView);

    {
        // Transparency set-up:
        m_quickWindow->setColor(Qt::transparent);

        QSurfaceFormat format = m_quickWindow->format();
        format.setAlphaBufferSize(8);
        m_quickWindow->setFormat(format);
    }

    // Make sure the UI child window has the same size as the root parent window:
    connect(m_rootWindow.get(), &QWindow::widthChanged, m_quickWindow, &QWindow::setWidth);
    connect(m_rootWindow.get(), &QWindow::heightChanged, m_quickWindow, &QWindow::setHeight);

    m_quickWindow->create();

    const bool ret = commonGUICreate(m_rootWindow.get(), m_quickWindow, CompositorVideo::CAN_SHOW_PIP | CompositorVideo::HAS_ACRYLIC);

    m_quickWindow->setFlag(Qt::FramelessWindowHint);
    // Qt QPA Bug (qwindows, qdirect2d(?)): to trigger WS_EX_LAYERED set up.
    m_quickWindow->setOpacity(0.0);
    m_quickWindow->setOpacity(1.0);

    m_rootWindow->installEventFilter(this);

    if (aboutToShowQuickWindowCallback)
        aboutToShowQuickWindowCallback(m_quickWindow);

#ifdef Q_OS_MACOS
    // Opt-in unattended probes use a small corner without normal UI size limits.
    if (qEnvironmentVariableIsSet("VLC_QT_NO_ACTIVATE"))
    {
        m_rootWindow->setMinimumSize(QSize(0, 0));
        const auto probeSize = qEnvironmentVariable("VLC_QT_PROBE_SIZE", "480x320").split('x');
        m_rootWindow->setGeometry(20, 100, probeSize.value(0).toInt(), probeSize.value(1).toInt());
    }
#endif
    m_rootWindow->setVisible(true);
    m_videoWindow->setVisible(true);
    m_quickWindow->setVisible(true);

    m_quickWindow->raise(); // Make sure quick window is above the video window.

#ifdef Q_OS_MACOS
    // Set background only after QPA has finished showing the transparent child
    // views. A backing-store image would overpaint native video; a parent layer
    // background sits below all child layers and fills uncovered letterboxes.
    const auto getObject = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend);
    const auto setObject = reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend);
    const auto setBool = reinterpret_cast<void (*)(id, SEL, bool)>(objc_msgSend);
    const id view = reinterpret_cast<id>(m_rootWindow->winId());
    const id window = getObject(view, sel_registerName("window"));
    const id black = getObject(reinterpret_cast<id>(objc_getClass("NSColor")), sel_registerName("blackColor"));
    setObject(window, sel_registerName("setBackgroundColor:"), black);
    setBool(window, sel_registerName("setOpaque:"), true);
    setBool(view, sel_registerName("setWantsLayer:"), true);
    setObject(getObject(view, sel_registerName("layer")), sel_registerName("setBackgroundColor:"),
              getObject(black, sel_registerName("CGColor")));

    // QNSView is flipped, whereas VLC's native renderers expect a Cocoa
    // bottom-left coordinate system (including convertRectFromBacking).
    // Keep Qt's child geometry in top-left logical coordinates, but hand vout
    // an unflipped NSView that tracks the child bounds on every resize.
    const id videoView = reinterpret_cast<id>(m_videoWindow->winId());
    const id nativeVideoView = getObject(getObject(reinterpret_cast<id>(objc_getClass("NSView")),
                                                   sel_registerName("alloc")), sel_registerName("init"));
    CGRect frame;
#if defined(__x86_64__)
    reinterpret_cast<void (*)(CGRect *, id, SEL)>(objc_msgSend_stret)(&frame, videoView, sel_registerName("bounds"));
#else
    frame = reinterpret_cast<CGRect (*)(id, SEL)>(objc_msgSend)(videoView, sel_registerName("bounds"));
#endif
    reinterpret_cast<void (*)(id, SEL, CGRect)>(objc_msgSend)(nativeVideoView, sel_registerName("setFrame:"), frame);
    // NSViewWidthSizable | NSViewHeightSizable
    reinterpret_cast<void (*)(id, SEL, unsigned long)>(objc_msgSend)(nativeVideoView, sel_registerName("setAutoresizingMask:"), 2 | 16);
    setObject(videoView, sel_registerName("addSubview:"), nativeVideoView);
    m_nativeVideoView = nativeVideoView;
    reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(nativeVideoView, sel_registerName("release")); // retained by videoView
#endif
    return ret;
}

void CompositorPlatform::destroyMainInterface()
{
    commonIntfDestroy();
}

void CompositorPlatform::unloadGUI()
{
    m_rootWindow->removeEventFilter(this);
    m_interfaceWindowHandler.reset();
    m_quickWindow->setSource(QUrl());
    commonGUIDestroy();
}

bool CompositorPlatform::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    if (m_wnd)
        return false;

    commonSetupVoutWindow(p_wnd, destroyCb);

#ifdef __WIN32
    p_wnd->type = VLC_WINDOW_TYPE_HWND;
    p_wnd->handle.hwnd = reinterpret_cast<void*>(m_videoWindow->winId());

    return true;
#endif

#ifdef __APPLE__
    p_wnd->type = VLC_WINDOW_TYPE_NSOBJECT;
#ifdef Q_OS_MACOS
    p_wnd->handle.nsobject = reinterpret_cast<id>(m_nativeVideoView);
#else
    p_wnd->handle.nsobject = reinterpret_cast<id>(m_videoWindow->winId());
#endif

    return true;
#endif

    vlc_assert_unreachable();
}

QWindow *CompositorPlatform::interfaceMainWindow() const
{
    return m_rootWindow.get();
}

QQuickWindow *CompositorPlatform::quickWindow() const
{
    return m_quickWindow.get();
}

Compositor::Type CompositorPlatform::type() const
{
    return Compositor::PlatformCompositor;
}

QQuickItem *CompositorPlatform::activeFocusItem() const
{
    assert(m_quickWindow);
    return m_quickWindow->activeFocusItem();
}

bool CompositorPlatform::eventFilter(QObject *watched, QEvent *event)
{
    // Forward drag events to the child quick window,
    // as it is not done automatically by Qt with
    // nested windows:
    if (m_quickWindow && watched == m_rootWindow.get())
    {
        switch (event->type()) {
        case QEvent::DragEnter:
        case QEvent::DragLeave:
        case QEvent::DragMove:
        case QEvent::DragResponse:
        case QEvent::Drop:
            QApplication::sendEvent(m_quickWindow, event);
            return true;
        default:
            break;
        };
    }
    return false;
}

void CompositorPlatform::commitSurface()
{
    if (!m_videoWindow)
        return;

    if (m_pendingPosition && m_pendingSize)
    {
        m_videoWindow->setGeometry(m_pendingPosition->x(), m_pendingPosition->y(),
                                   m_pendingSize->width(), m_pendingSize->height());
        m_pendingPosition.reset();
        m_pendingSize.reset();
    }
    else
    {
        if (m_pendingSize)
        {
            m_videoWindow->resize(*m_pendingSize);
            m_pendingSize.reset();
        }
        else if (m_pendingPosition)
        {
            m_videoWindow->setPosition(*m_pendingPosition);
            m_pendingPosition.reset();
        }
    }
}

int CompositorPlatform::windowEnable(const vlc_window_cfg_t *cfg)
{
#ifdef Q_OS_MACOS
    // The renderer can prepare its first native frame before QML reports the
    // video item bounds. Give it a valid initial backing-sized host; otherwise
    // an unchanged first placement (e.g. a 1920x1080 viewport) stays stale.
    if (cfg->width && cfg->height)
    {
        const qreal dpr = m_videoWindow->devicePixelRatio();
        m_videoWindow->resize(std::ceil(cfg->width / dpr), std::ceil(cfg->height / dpr));
    }
#else
    Q_UNUSED(cfg);
#endif
    commonWindowEnable();
    return VLC_SUCCESS;
}

void CompositorPlatform::windowDisable()
{
    commonWindowDisable();
}

void CompositorPlatform::onSurfacePositionChanged(const QPointF &position)
{
    const QPointF point = position / m_videoWindow->devicePixelRatio();
    m_pendingPosition = QPoint{static_cast<int>(point.x()), static_cast<int>(point.y())};
}

void CompositorPlatform::onSurfaceSizeChanged(const QSizeF &size)
{
    const QSizeF area = (size / m_videoWindow->devicePixelRatio());
    m_pendingSize = QSize{static_cast<int>(std::ceil(area.width())), static_cast<int>(std::ceil(area.height()))};
}
