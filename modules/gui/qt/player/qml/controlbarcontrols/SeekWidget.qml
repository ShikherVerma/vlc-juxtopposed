/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Player as PlayerWidgets

T.Control {
    id: root
    property bool paintOnly: false
    property bool remainingDuration: false

    Layout.minimumWidth: VLCStyle.dp(240, VLCStyle.scale)
    implicitWidth: VLCStyle.dp(paintOnly ? 320 : 10000, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    function format(tick) {
        const text = tick.formatHMS(0)
        return tick.isSubHour() ? "00:" + text : text
    }

    RowLayout {
        anchors.fill: parent
        spacing: VLCStyle.dp(8, VLCStyle.scale)

        T.Label {
            readonly property bool hovered: elapsedHover.hovered
            HoverHandler { id: elapsedHover }
            background: Rectangle {
                radius: VLCStyle.control_radius
                color: parent.hovered ? slider.colorContext.bg.primary : "transparent"
            }
            Layout.preferredWidth: VLCStyle.dp(76, VLCStyle.scale)
            Layout.fillHeight: true
            text: root.paintOnly ? "00:02:10" : (MainCtx.showRemainingTime ? "-" + root.format(Player.remainingTime) : root.format(Player.time))
            color: theme.fg.primary
            font.family: VLCStyle.fontFamily
            font.pixelSize: VLCStyle.fontSize_timecode
            font.weight: Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Accessible.name: qsTr("Current or remaining time")
            Accessible.role: Accessible.Button
            Accessible.onPressAction: MainCtx.showRemainingTime = !MainCtx.showRemainingTime
            TapHandler { enabled: !root.paintOnly; onTapped: MainCtx.showRemainingTime = !MainCtx.showRemainingTime }
        }

        PlayerWidgets.SliderBar {
            id: slider
            Layout.fillWidth: true
            Layout.fillHeight: true
            barHeight: VLCStyle.dp(28, VLCStyle.scale)
            thick: true
            enabled: !root.paintOnly && Player.seekable
            focus: true
            activeFocusOnTab: true
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Progress")
            AccessibleCompat.id: "progressSlider"
            Navigation.parentItem: root
            Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
        }

        T.Label {
            readonly property bool hovered: durationHover.hovered
            HoverHandler { id: durationHover }
            background: Rectangle {
                radius: VLCStyle.control_radius
                color: parent.hovered ? slider.colorContext.bg.primary : "transparent"
            }
            Layout.preferredWidth: VLCStyle.dp(76, VLCStyle.scale)
            Layout.fillHeight: true
            text: root.paintOnly ? "00:25:40" : (root.remainingDuration ? "-" + root.format(Player.remainingTime) : root.format(Player.length))
            color: theme.fg.primary
            font: parent.children[0].font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Accessible.name: qsTr("Duration or remaining time")
            Accessible.role: Accessible.Button
            Accessible.onPressAction: root.remainingDuration = !root.remainingDuration
            TapHandler { enabled: !root.paintOnly; onTapped: root.remainingDuration = !root.remainingDuration }
        }
    }
}
