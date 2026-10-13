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
import QtQuick.Controls.Basic
import QtQuick.Templates as T


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + spacing + implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxxsmall
    leftPadding: VLCStyle.option_padding + padding
    rightPadding: leftPadding
    spacing: VLCStyle.margin_xsmall
    font.family: VLCStyle.fontFamily
    font.pixelSize: VLCStyle.fontSize_normal
    property real rowHeight: VLCStyle.option_height

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox

        focused: control.activeFocus
        enabled: control.enabled
        hovered: control.hovered
        pressed: control.pressed
    }

    property color color: theme.fg.primary
    property color bgColor: theme.bg.primary
    property color borderColor: theme.border

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    delegate: T.ItemDelegate {
        id: option
        width: control.popup.availableWidth
        height: control.rowHeight - VLCStyle.margin_xxsmall
        leftPadding: VLCStyle.option_padding
        rightPadding: leftPadding
        topPadding: VLCStyle.margin_xxxsmall
        bottomPadding: topPadding
        readonly property ColorContext colorContext: ColorContext {
            id: optionTheme
            colorSet: ColorContext.Item
            palette: control.colorContext.palette
            enabled: option.enabled
            hovered: option.hovered || option.highlighted
            focused: option.visualFocus
            pressed: option.down
        }
        readonly property ColorContext labelColorContext: ColorContext {
            id: labelTheme
            colorSet: ColorContext.ComboBox
            palette: control.colorContext.palette
            enabled: option.enabled
        }
        contentItem: Widgets.ListLabel {
            text: control.textAt(index)
            color: labelTheme.fg.primary
            font: control.font
            rightPadding: VLCStyle.icon_normal + VLCStyle.margin_xsmall
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Widgets.IconLabel {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: VLCIcons.check
                color: optionTheme.accent
                font.pixelSize: VLCStyle.icon_normal
                visible: index === control.currentIndex
            }
        }
        background: Widgets.AnimatedBackground {
            radius: VLCStyle.button_radius
            color: index === control.currentIndex ? control.colorContext.bg.secondary : optionTheme.bg.primary
            border.color: option.visualFocus ? optionTheme.accent : "transparent"
        }
        highlighted: control.highlightedIndex === index
    }

    indicator: Widgets.IconLabel {
        x: control.width - width - rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        font.pixelSize: VLCStyle.icon_normal
        font.bold: true
        text: VLCIcons.expand
        color: control.color
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    contentItem: Widgets.ListLabel {
        rightPadding: control.spacing + control.indicator.width
        font.family: VLCStyle.fontFamily
        font.pixelSize: VLCStyle.fontSize_normal
        font.weight: Font.Normal

        text: control.displayText
        color: control.color
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: control.bgColor
        implicitHeight: VLCStyle.dropdown_height
        border.color: control.activeFocus ? theme.accent : control.borderColor
        border.width: VLCStyle.margin_xxxsmall
        radius: VLCStyle.control_radius
    }

    popup: Popup {
        y: control.height + VLCStyle.margin_xsmall

        // NOTE: This Popup should be on top of other Popup(s) most of the time.
        z: 100

        // Narrow toolbar combos must not truncate every option merely because
        // the authored option padding/check slot are wider than the closed box.
        width: Math.max(control.width, VLCStyle.dp(200, VLCStyle.scale))
        padding: VLCStyle.margin_xxxsmall

        contentItem: ListView {
            clip: true
            spacing: VLCStyle.margin_xxxsmall
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollBar.vertical: Widgets.ScrollBarExt { }
        }

        background: Widgets.PopupBackground {
            colorContext.palette: theme.palette
            radius: VLCStyle.dropdownPopup_radius
        }
    }
}
