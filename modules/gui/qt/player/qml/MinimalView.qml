/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQuick

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Menus

FocusScope {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        palette: VLCStyle.palette

        colorSet: ColorContext.Window
    }

    VideoSurface {
        id: videoSurface

        focus: visible

        videoSurfaceProvider: MainCtx.videoSurfaceProvider

        visible: MainCtx.hasEmbededVideo

        anchors.fill: parent

        onMouseMoved: {
            mouseAutoHide.restart()
            videoSurface.cursorShape = Qt.ArrowCursor
        }

        onVisibleChanged: {
            mouseAutoHide.restart()
            videoSurface.cursorShape = Qt.ArrowCursor
        }

        Timer {
            // toggleControlBarButton's visibility depends on this timer
            id: mouseAutoHide
            running: true
            repeat: false
            interval: MainCtx.mouseHideTimeout

            onTriggered: {
                // Cursor hides when toggleControlBarButton is not visible
                videoSurface.forceActiveFocus()
                videoSurface.cursorShape = Qt.BlankCursor
            }
        }
    }

    // TODO: `ViewBlockingRectangle` (`AcrylicBackground`) is not necessary
    //       here, this should ideally be a translucent rectangle. It is
    //       is only used to provide the tint color and animations.
    Widgets.AcrylicBackground {
        tintColor: theme.bg.primary

        anchors.fill: parent

        visible: !MainCtx.hasEmbededVideo
        focus: visible

        Image {
            id: logo

            source: MainCtx.useXmasCone()
                    ? "qrc:///logo/vlc48-xmas.png"
                    : SVGColorImage.colorize("qrc:///misc/cone.svg").accent(VLCStyle.palette.accentLogo).uri()

            anchors.centerIn: parent
            width: Math.min(parent.width / 2, sourceSize.width)
            height: Math.min(parent.height / 2, sourceSize.height)
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: MainCtx.bgCone
        }

        QmlAudioContextMenu {
            id: audioContextMenu
            ctx: MainCtx
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (eventPoint, button) => {
                audioContextMenu.popup(eventPoint.globalPosition)
            }
        }
    }

    Keys.onPressed: (event) => {
        if (event.accepted)
            return
        MainCtx.sendHotkey(event.key, event.modifiers);
    }
}
