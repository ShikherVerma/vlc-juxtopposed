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

import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

T.ItemDelegate {
    id: control

    // Properties

    property string iconTxt: ""

    property bool showText: true

    property real radius: VLCStyle.dp(4, VLCStyle.scale)

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    focusPolicy: Qt.StrongFocus

    leftInset: implicitLeftInset
    rightInset: implicitRightInset

    topInset: implicitTopInset
    bottomInset: implicitBottomInset

    property real implicitLeftInset: 0
    property real implicitRightInset: 0

    property real implicitTopInset: 0
    property real implicitBottomInset: 0

    // Accessible

    Accessible.onPressAction: control.clicked()
    Accessible.name: text
    Accessible.role: Accessible.Button
    Accessible.selected: highlighted || checked
    property int navigationDepth: 0

    // Tooltip

    T.ToolTip.visible: (showText === false && T.ToolTip.text && (hovered || visualFocus))

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    T.ToolTip.text: text

    // Childs

    ColorContext {
        id: theme
        colorSet: ColorContext.TabButton

        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
        enabled: control.enabled
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"

        radius: control.radius

    }

    contentItem: RowLayout {
        spacing: VLCStyle.dp(8, VLCStyle.scale)

        Item {
            Layout.preferredWidth: VLCStyle.dp(control.iconTxt === VLCIcons.topbar_video ? 32 : 30, VLCStyle.scale)
            visible: control.navigationDepth === 0
            Layout.fillHeight: true

            Widgets.IconLabel {
                id: iconLabel

                visible: text.length > 0

                anchors.centerIn: parent

                text: {
                    if (!(control.highlighted || control.checked))
                        return control.iconTxt
                    switch (control.iconTxt) {
                    case VLCIcons.home: return VLCIcons.home_state_active
                    case VLCIcons.topbar_video: return VLCIcons.videos_state_active
                    case VLCIcons.topbar_music: return VLCIcons.songs_state_active
                    case VLCIcons.topbar_network: return VLCIcons.streaming_state_active
                    case VLCIcons.media_playlist: return VLCIcons.playlist_state_active
                    case VLCIcons.settings: return VLCIcons.settings_state_active
                    default: return control.iconTxt
                    }
                }

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                color: (control.highlighted || control.checked) ? VLCStyle.accentForeground(VLCStyle.palette) : theme.fg.primary

                font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
            }
        }

        T.Label {
            id: label

            Layout.fillWidth: true
            Layout.fillHeight: true

            text: control.text

            verticalAlignment: Text.AlignVCenter

            color: (control.highlighted || control.checked) ? theme.fg.secondary : theme.fg.primary

            elide: Text.ElideRight

            visible: control.showText

            font.pixelSize: VLCStyle.fontSize_normal

            font.weight: Font.Normal

            //button text is already exposed
            Accessible.ignored: true

            Behavior on color {
                enabled: theme.initialized

                ColorAnimation {
                    duration: VLCStyle.duration_short
                }
            }
        }
    }
}
