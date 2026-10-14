/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Templates as T
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.Util

T.Control {
    id: root
    objectName: "pip window"
    property bool showVideo: Player.hasVideoOutput && MainCtx.hasEmbededVideo
    property bool hideControls: MainCtx.usingTouch
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    readonly property bool controlsVisible: !hideControls && (MainCtx.usingTouch || hover.hovered || activeFocus || controlFocus.activeFocus)
    Accessible.role: Accessible.Graphic
    Accessible.name: showVideo ? qsTr("Video content") : qsTr("Album artwork")

    HoverHandler { id: hover }
    DoubleClickIgnoringItem {
        anchors.fill: parent
        TapHandler {
            onDoubleTapped: MainCtx.playerView = true
            onTapped: {
                if (MainCtx.usingTouch) root.hideControls = !root.hideControls
                else MainPlaylistController.togglePlayPause()
            }
        }
    }

    // The main player is unloaded before this surface is created. This uses
    // the same provider/vout, never a second player or media input.
    background: Rectangle {
        color: frameTheme.bg.secondary
        radius: VLCStyle.dp(3)
        ColorContext { id: frameTheme; palette: VLCStyle.palette; colorSet: ColorContext.View }
        Loader {
            anchors.fill: parent
            anchors.margins: VLCStyle.dp(2)
            sourceComponent: root.showVideo ? video : artwork
        }
        Component {
            id: video
            VideoSurface {
                videoSurfaceProvider: MainCtx.videoSurfaceProvider
            }
        }
        Component {
            id: artwork
            Widgets.MediaCover {
                source: Player.artwork
                fallbackImageSource: VLCStyle.noArtAlbumCover
                pictureWidth: width
                pictureHeight: height
                fillMode: Image.PreserveAspectFit
                radius: VLCStyle.dp(3)
            }
        }
    }

    contentItem: FocusScope {
        id: controlFocus
        z: 1
        visible: root.controlsVisible
        Rectangle { anchors.fill: parent; color: "#80000000" }
        // Controls over video/art deliberately keep light ink in either theme.
        // Light page controls outside this surface use the application palette.
        component VideoButton: Widgets.IconButton {
            width: VLCStyle.dp(36); height: width
            font.pixelSize: VLCStyle.dp(24)
            color: enabled ? "#eaeaea" : "#8f8f8f"
            background: Rectangle {
                color: parent.down ? "#373737" : parent.hovered ? "#222222" : "transparent"
                radius: VLCStyle.dp(4)
                border.width: parent.visualFocus ? 1 : 0
                border.color: VLCStyle.palette.accentMain
            }
        }
        VideoButton {
            anchors { left: parent.left; top: parent.top; margins: VLCStyle.dp(8) }
            text: VLCIcons.close
            description: qsTr("Stop and close mini-player")
            AccessibleCompat.id: "pipCloseButton"
            onClicked: MainPlaylistController.stop()
        }
        VideoButton {
            anchors { right: parent.right; top: parent.top; margins: VLCStyle.dp(8) }
            text: VLCIcons.fullscreen
            description: qsTr("Return to player")
            AccessibleCompat.id: "pipFullscreenButton"
            onClicked: MainCtx.playerView = true
        }
        Row {
            anchors.centerIn: parent
            spacing: VLCStyle.dp(32)
            VideoButton {
                text: VLCIcons.previous
                description: qsTr("Previous")
                enabled: MainPlaylistController.hasPrev
                onClicked: MainPlaylistController.prev()
            }
            VideoButton {
                text: Player.playingState === Player.PLAYING_STATE_PLAYING ? VLCIcons.pause_filled : VLCIcons.play_filled
                description: qsTr("Play / pause")
                AccessibleCompat.id: "pipPlayButton"
                onClicked: MainPlaylistController.togglePlayPause()
            }
            VideoButton {
                text: VLCIcons.next
                description: qsTr("Next")
                enabled: MainPlaylistController.hasNext
                onClicked: MainPlaylistController.next()
            }
        }
    }
}
