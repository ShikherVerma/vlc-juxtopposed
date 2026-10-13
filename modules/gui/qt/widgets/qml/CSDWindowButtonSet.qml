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
import QtQuick.Window


import VLC.MainInterface
import VLC.Style

Row {
    id: windowButtonGroup

    spacing: 0
    padding: 0

    width: implicitWidth

    property bool playerStyle: false

    property color color: theme.fg.primary
    property color hoverColor: theme.bg.primary.alpha(0.5)

    readonly property bool useWinIcons: !playerStyle && ((MainCtx.osName === MainCtx.Windows)&&(MainCtx.osVersion >= 10))

    readonly property bool buttonHovered: {
        let h = false
        for (let i = 0; i < repeater.count; ++i) {
            const button = repeater.itemAt(i)
            h = h || button.buttonHovered
        }

        return h
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: windowButtonGroup.playerStyle ? ColorContext.ToolButton : ColorContext.Window
    }

    Repeater {
        id: repeater

        model: MainCtx.csdButtonModel.windowCSDButtons

        Row {
            id: buttonRow
            required property var modelData
            readonly property bool buttonHovered: windowButton.hovered || windowButton.showHovered || fullscreenButton.hovered

            CSDWindowButton {
                id: fullscreenButton
                visible: windowButtonGroup.playerStyle && buttonRow.modelData.type === CSDButton.Close
                width: visible ? VLCStyle.dp(36, VLCStyle.scale) : 0
                height: windowButtonGroup.height
                authoredIcon: windowButtonGroup.playerStyle
                iconTxt: MainCtx.intfMainWindow.visibility === Window.FullScreen ? VLCIcons.defullscreen : VLCIcons.fullscreen
                color: windowButtonGroup.color
                hoverColor: windowButtonGroup.hoverColor
                isThemeDark: theme.palette.isDark
                Accessible.name: qsTr("Fullscreen")
                onClicked: MainCtx.toggleInterfaceFullScreen()
            }

            CSDWindowButton {
                id: windowButton
                property var modelData: buttonRow.modelData
                width: windowButtonGroup.playerStyle ? VLCStyle.dp(36, VLCStyle.scale) : VLCStyle.dp(40, VLCStyle.scale)
                authoredIcon: windowButtonGroup.playerStyle
                height: windowButtonGroup.height

                showHovered: modelData.showHovered

                externalPressed: modelData.externalPressed

                color: (!windowButtonGroup.playerStyle && modelData.type === CSDButton.Close && (hovered || modelData.showHovered))
                       ? "white"
                       : windowButtonGroup.color

                hoverColor: (!windowButtonGroup.playerStyle && modelData.type === CSDButton.Close) ? "red" : windowButtonGroup.hoverColor

                isThemeDark: theme.palette.isDark

                iconTxt:{
                    switch (modelData.type) {
                    case CSDButton.Minimize:
                    {
                        if (useWinIcons)
                            return "\uE921"

                        else
                            return VLCIcons.window_minimize
                    }

                    case CSDButton.MaximizeRestore:
                    {
                        if (useWinIcons)
                            return (MainCtx.intfMainWindow.visibility === Window.Maximized)
                                    ? "\uE923"
                                    : "\uE922"

                        else
                            return (MainCtx.intfMainWindow.visibility === Window.Maximized)
                                    ? VLCIcons.window_restore
                                    : VLCIcons.window_maximize
                    }

                    case CSDButton.Close:
                    {
                        if (useWinIcons)
                            return "\uE8BB"

                        else
                            return VLCIcons.window_close
                    }
                    }
                    console.assert(false, "unreachable")
                }

                Accessible.name: modelData.type === CSDButton.Close ? qsTr("Close") : modelData.type === CSDButton.Minimize ? qsTr("Minimize") : qsTr("Maximize or restore")
                onClicked: modelData.click()

                // handles VLCStyle.scale changes
                onWidthChanged: Qt.callLater(windowButton.updateRect)
                onHeightChanged: Qt.callLater(windowButton.updateRect)
                onVisibleChanged: Qt.callLater(windowButton.updateRect)
                Component.onDestruction: {
                    if (modelData)
                        modelData.rect = Qt.rect(0, 0, 0, 0)
                }

                Connections {
                    target: VLCStyle

                    // handle window resize
                    function onAppWidthChanged() { Qt.callLater(windowButton.updateRect) }
                    function onAppHeightChanged() { Qt.callLater(windowButton.updateRect) }
                }

                function updateRect() {
                    const point = mapToItem(null, 0, 0)
                    const rect = visible ? Qt.rect(point.x, point.y, width, height) : Qt.rect(0, 0, 0, 0)

                    modelData.rect = rect
                }
            }
        }
    }
}
