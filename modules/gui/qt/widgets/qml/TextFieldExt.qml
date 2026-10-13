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
import QtQuick.Controls.Basic


import VLC.Style

TextField {
    id: control

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.TextField

        focused: control.activeFocus
        hovered: control.hovered
        enabled: control.enabled
    }

    selectedTextColor : theme.fg.highlight
    selectionColor : theme.bg.highlight
    color : theme.fg.primary
    placeholderTextColor: theme.fg.secondary

    font.family: VLCStyle.fontFamily
    font.pixelSize: VLCStyle.fontSize_normal
    padding: VLCStyle.margin_xsmall
    selectByMouse: true

    verticalAlignment: Text.AlignVCenter

    property real radius: VLCStyle.control_radius
    property color bgColor: theme.bg.primary

    background: Rectangle {
        implicitWidth: VLCStyle.widthSearchInput
        implicitHeight: VLCStyle.control_height
        border.width: VLCStyle.border
        color: control.bgColor
        border.color: theme.border
        border.pixelAligned: (radius < Number.EPSILON)
        radius: control.radius
    }

}
