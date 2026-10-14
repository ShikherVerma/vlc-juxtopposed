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
import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

T.Control {
    id: root
    Accessible.name: qsTr("Mini player")
    AccessibleCompat.id: "miniplayerControlBar"
    property Item dragTarget: root
    property real dragXMin: 0
    property real dragXMax: 0
    property real dragYMin: 0
    property real dragYMax: 0
    readonly property real sliderY: contentItem.y + progress.y
    readonly property ColorContext colorContext: ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.Window }
    implicitWidth: VLCStyle.dp(391)
    implicitHeight: video.height + VLCStyle.dp(130)
    padding: VLCStyle.dp(8)
    background: Rectangle { color: theme.bg.primary; radius: VLCStyle.dp(14) }

    contentItem: Item {
        PIPPlayer {
            id: video
            width: parent.width
            height: width * 9 / 16
        }
        SliderBar {
            id: progress
            anchors { top: video.bottom; topMargin: VLCStyle.dp(6); left: parent.left; right: parent.right }
            height: VLCStyle.dp(16)
            barHeight: height
            thick: true
            compactHandle: true
            enabled: Player.seekable
            activeFocusOnTab: true
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Mini-player progress")
        }
        Widgets.MenuLabel {
            id: title
            // VLC already supports dragging its docked PIP. Move the whole card from
            // its title region, leaving transport and seeking gestures independent.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.DragMoveCursor
                drag.target: root.dragTarget
                drag.threshold: 0
                drag.minimumX: root.dragXMin; drag.maximumX: root.dragXMax
                drag.minimumY: root.dragYMin; drag.maximumY: root.dragYMax
                onPressed: {
                    root.dragTarget.anchors.bottom = undefined
                    root.dragTarget.anchors.right = undefined
                    console.debug("Mini-player drag:", true, root.dragTarget.x, root.dragTarget.y)
                }
                onReleased: console.debug("Mini-player drag:", false, root.dragTarget.x, root.dragTarget.y)
            }
            anchors { top: progress.bottom; topMargin: VLCStyle.dp(20); left: parent.left; leftMargin: VLCStyle.dp(8); right: parent.right; rightMargin: VLCStyle.dp(8) }
            height: VLCStyle.dp(24)
            text: Player.title
            color: theme.fg.primary
            font.pixelSize: VLCStyle.dp(16)
            font.weight: Font.Normal
            elide: Text.ElideMiddle
        }
        RowLayout {
            anchors { top: title.bottom; topMargin: VLCStyle.dp(8); left: title.left; right: title.right }
            height: VLCStyle.dp(32)
            spacing: VLCStyle.dp(8)
            Widgets.IconLabel { text: VLCIcons.folder; color: theme.fg.primary; font.pixelSize: VLCStyle.dp(24) }
            Widgets.MenuLabel {
                Layout.fillWidth: true
                text: {
                    const url = Player.url.toString()
                    if (!url.startsWith("file:")) return qsTr("Stream")
                    const parts = decodeURIComponent(url).split("/")
                    return parts.length > 1 ? parts[parts.length - 2] : ""
                }
                color: theme.fg.primary
                font.pixelSize: VLCStyle.dp(14)
                elide: Text.ElideMiddle
            }
            Widgets.MenuLabel {
                text: Player.time.formatHMS(0) + " / " + (Player.length.valid() && !Player.length.isSubSecond() ? Player.length.formatHMS(0) : "—")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.dp(12)
                Accessible.name: qsTr("Elapsed time / duration")
            }
            Widgets.IconToolButton {
                text: VLCIcons.playlist
                description: qsTr("Show queue")
                checked: MainCtx.playqueuePanel.visible
                onClicked: MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible
            }
        }
    }
}
