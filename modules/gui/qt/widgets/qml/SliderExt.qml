/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

T.Slider {
    id: control

    // helper to set colors based on given particular theme
    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.Slider

        enabled: control.enabled
        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.pressed
    }

    // colors based on different parts and states
    property color color: control.enabled ? (theme.palette?.accentDim ?? VLCStyle.palette.accentDim) : theme.fg.primary
    property color fillColor: theme.bg.secondary
    property color bgColor: control.enabled ? theme.bg.primary
                                            : ((theme.palette?.isDark ?? VLCStyle.palette.isDark)
                                               ? theme.bg.primary : "#dedede")

    property real handleWidth: VLCStyle.icon_xsmall

    property real radius: VLCStyle.control_radius

    // when set the tooltip will follow the mouse when control is hoverred
    // else tooltip will always be shown at current value.
    property bool toolTipFollowsMouse: false

    // toolTipTextProvider -> function(value)
    // arg "value" is between from and to, this is "value"
    // at which pointing tool tip is currently shown
    //
    // returns the text for the given value
    property var toolTipTextProvider: function (value) {
        return value
    }

    readonly property real _tooltipX: {
        if (toolTipFollowsMouse && hoverHandler.hovered)
            return  hoverHandler.point.position.x

        return (handle.x + handle.width / 2) // handle center
    }

    // find position under given x, can be used with Slider::valueAt()
    // x is coordinate in this control's coordinate space
    function positionAt(x) {
        const hw = control.horizontal ? control.handle.width : control.handle.height
        const offset = (control.horizontal ? control.leftPadding : control.topPadding) + hw / 2
        const extend = (control.horizontal ? control.availableWidth : control.availableHeight) - hw
        const p = extend > 0 ? (x - offset) / extend : 0
        return control.horizontal ? (control.mirrored ? 1 - p : p) : 1 - p
    }

    padding: 0

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal
                                  ? control.visualPosition * (control.availableWidth - width)
                                  : (control.availableWidth - width) / 2)

        y: control.topPadding + (control.horizontal
                                 ? (control.availableHeight - height) / 2
                                 : control.visualPosition * (control.availableHeight - height))

        implicitWidth: control.horizontal ? control.handleWidth : VLCStyle.slider_thumbSize
        implicitHeight: control.horizontal ? VLCStyle.slider_thumbSize : control.handleWidth
        radius: VLCStyle.slider_radius
        color: control.color
        border.width: control.visualFocus ? VLCStyle.focus_border : 0
        border.color: theme.accent
    }

    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)

        implicitWidth: control.horizontal ? VLCStyle.dp(320, VLCStyle.scale) : VLCStyle.slider_trackSize
        implicitHeight: control.horizontal ? VLCStyle.slider_trackSize : VLCStyle.dp(320, VLCStyle.scale)

        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight

        radius: control.radius

        color: control.bgColor
        scale: control.horizontal && control.mirrored ? -1 : 1

        Rectangle {
            x: VLCStyle.margin_xxxsmall
            y: control.horizontal ? VLCStyle.margin_xxxsmall : VLCStyle.margin_xxxsmall + (1 - control.position) * (parent.height - VLCStyle.margin_xxsmall)
            width: control.horizontal ? control.position * (parent.width - VLCStyle.margin_xxsmall) : parent.width - VLCStyle.margin_xxsmall
            height: control.horizontal ? parent.height - VLCStyle.margin_xxsmall : control.position * (parent.height - VLCStyle.margin_xxsmall)
            radius: VLCStyle.slider_radius
            color: control.fillColor
        }
    }

    HoverHandler {
        id: hoverHandler

        enabled: true

        target: background
    }

    Widgets.PointingToolTipAttached.pos: control.horizontal
        ? Qt.point(control._tooltipX, control.handle.height / 2)
        : Qt.point(control.handle.width / 2, toolTipFollowsMouse && hoverHandler.hovered
                   ? hoverHandler.point.position.y : control.handle.y + control.handle.height / 2)
    Widgets.PointingToolTipAttached.visible: (hoverHandler.hovered || control.visualFocus || control.pressed)
    Widgets.PointingToolTipAttached.text: {
        if (!Widgets.PointingToolTipAttached.instance?.visible)
            return ""

        // position is only measured till half of handle width
        // pos.x may go beyond the position at the edges
        const tip = Widgets.PointingToolTipAttached.instance.pos
        const p = Helpers.clamp(control.positionAt(control.horizontal ? tip.x : tip.y), 0.0, 1.0)
        const v = control.valueAt(p)
        return control.toolTipTextProvider(v)
    }

}
