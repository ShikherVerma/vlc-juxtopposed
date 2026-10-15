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
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQuick.Window


import VLC.MainInterface
import VLC.Style
import VLC.PlayerControls
import VLC.Playlist
import VLC.Widgets as Widgets
import VLC.Menus as Menus
import VLC.Util

FocusScope {
    id: root

    property int textWidth: width
    property Item plListView
    readonly property int reservedHeight: implicitHeight
    property int topMargin: 0
    property int sideMargin: 0
    property string title: qsTr("VLC Media Player")
    property string location: qsTr("Videos")
    property bool showCSD: false
    property bool showToolbar: false
    property bool pinControls: false
    property bool navigationVisible: false
    property bool showLocation: true
    property bool showNavigationToggle: showLocation
    property bool playlistVisible: false

    readonly property bool macCSD: Qt.platform.os === "osx" && root.showCSD
                                   && MainCtx.intfMainWindow.visibility !== Window.FullScreen
    // Keep the authored content to the right of the macOS traffic lights.
    readonly property int contentInset: macCSD ? VLCStyle.dp(78, VLCStyle.scale) : 0

    signal togglePlaylistVisibility()
    signal requestLockUnlockAutoHide(bool lock)
    signal backRequested()
    signal forceUnlock()

    implicitHeight: VLCStyle.dp(40, VLCStyle.scale) + (root.showToolbar ? menubar.implicitHeight : 0)
    height: implicitHeight
    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Player topbar")
    AccessibleCompat.id: "playerTopBar"

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    Rectangle {
        anchors.fill: parent
        color: windowTheme.bg.highlight
        ColorContext { id: windowTheme; colorSet: ColorContext.Window }
    }

    Loader {
        id: titleDrag
        anchors.fill: parent
        active: root.showCSD && MainCtx.intfMainWindow.visibility !== Window.FullScreen
        source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"
        Connections {
            target: titleDrag.item
            function onCsdMenuVisibleChanged() {
                root.requestLockUnlockAutoHide(titleDrag.item.csdMenuVisible)
            }
        }
    }

    // CSD counterpart of the native macOS close/minimize/fullscreen group.
    // The titlebar drag handler below retains double-click zoom/restore.
    Row {
        id: trafficLights
        visible: root.macCSD
        x: VLCStyle.dp(10, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        height: VLCStyle.dp(24, VLCStyle.scale)
        Repeater {
            model: [qsTr("Close"), qsTr("Minimize"), qsTr("Fullscreen")]
            T.Button {
                id: trafficButton
                required property int index
                required property string modelData
                width: VLCStyle.dp(20, VLCStyle.scale)
                height: trafficLights.height
                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.name: modelData
                background: Rectangle {
                    anchors.centerIn: parent
                    width: VLCStyle.dp(12, VLCStyle.scale)
                    height: width
                    radius: width / 2
                    color: trafficButton.down ? Qt.darker(["#ff5f57", "#febc2e", "#28c840"][trafficButton.index], 1.2)
                                              : ["#ff5f57", "#febc2e", "#28c840"][trafficButton.index]
                    border.width: trafficButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                    border.color: theme.fg.primary
                }
                contentItem: Widgets.IconLabel {
                    text: [VLCIcons.window_close, VLCIcons.window_minimize, VLCIcons.fullscreen][trafficButton.index]
                    font.pixelSize: VLCStyle.dp(12, VLCStyle.scale)
                    color: "#161616"
                    opacity: trafficButton.hovered || trafficButton.visualFocus ? 1 : 0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (index === 0)
                        MainCtx.intfMainWindow.close()
                    else if (index === 1)
                        MainCtx.requestInterfaceMinimized()
                    else
                        MainCtx.toggleInterfaceFullScreen()
                }
            }
        }
    }

    Widgets.BannerCone {
        x: root.contentInset + VLCStyle.dp(13, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        width: VLCStyle.dp(24, VLCStyle.scale)
        height: width
        sourceSize: Qt.size(width, height)
        color: VLCStyle.palette.accentLogo
    }

    Widgets.IconToolButton {
        id: menuSelector
        x: root.contentInset + VLCStyle.dp(51, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        width: VLCStyle.dp(28, VLCStyle.scale)
        height: width
        font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
        text: VLCIcons.more
        description: qsTr("Menu")
        AccessibleCompat.id: "topBarMenuButton"
        focus: true
        checked: contextMenu.shown
        Navigation.parentItem: root
        Navigation.rightItem: sidebarButton
        onClicked: contextMenu.popup(mapToGlobal(0, height))

        Menus.QmlGlobalMenu {
            id: contextMenu
            ctx: MainCtx
            onAboutToShow: root.requestLockUnlockAutoHide(true)
            onAboutToHide: root.requestLockUnlockAutoHide(false)
        }
        Connections {
            target: root
            function onForceUnlock() { contextMenu.close() }
        }
    }

    Widgets.IconToolButton {
        id: sidebarButton
        x: root.contentInset + VLCStyle.dp(89, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        width: VLCStyle.dp(28, VLCStyle.scale)
        height: width
        font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
        text: root.navigationVisible ? VLCIcons.sidebar_state_active : VLCIcons.panel_left
        description: root.showLocation ? qsTr("Show library")
                                       : (root.navigationVisible ? qsTr("Collapse library navigation") : qsTr("Expand library navigation"))
        AccessibleCompat.id: "sidebarToggleButton"
        visible: root.showNavigationToggle
        // Tool buttons normally take Tab focus only. In the library the
        // collapse/expand button also retains mouse focus for Space activation.
        focusPolicy: root.showLocation ? Qt.TabFocus : Qt.StrongFocus
        checked: root.navigationVisible
        Navigation.parentItem: root
        Navigation.leftItem: menuSelector
        Navigation.rightItem: root.showLocation ? locationButton : null
        onClicked: root.backRequested()
        // ToolTipExt normally opens above its owner. At this top-edge button
        // Popup.Window clamps it over the button and intercepts mouse presses.
        // Move only this library-owned popup below; restore the shared binding
        // when the tooltip changes owner (or this is the player topbar).
        Binding {
            target: sidebarButton.T.ToolTip.toolTip
            property: "y"
            when: !root.showLocation && !!sidebarButton.T.ToolTip.toolTip
                  && sidebarButton.T.ToolTip.toolTip.parent === sidebarButton
            value: sidebarButton.height + VLCStyle.margin_xxxsmall
        }
    }

    Widgets.IconLabel {
        x: root.contentInset + VLCStyle.dp(117, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        width: VLCStyle.dp(24, VLCStyle.scale)
        height: width
        font.pixelSize: width
        text: VLCIcons.divider
        color: VLCStyle.titlebarDividerColor(theme.palette.isDark)
        visible: root.showLocation
    }

    Widgets.TextToolButton {
        id: locationButton
        x: root.contentInset + VLCStyle.dp(141, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        height: VLCStyle.dp(28, VLCStyle.scale)
        width: Math.min(implicitWidth, Math.max(0, root.width / 2 - x - VLCStyle.dp(80, VLCStyle.scale)))
        visible: root.showLocation
        text: root.location
        padding: 0
        Accessible.name: text
        hoverEnabled: true
        Widgets.ToolTipExt {
            colorContext.palette: VLCStyle.palette
            text: locationButton.text
            visible: locationButton.hovered || locationButton.visualFocus
            delay: VLCStyle.delayToolTipAppear
            y: locationButton.height + VLCStyle.dp(4, VLCStyle.scale)
            width: Math.min(implicitWidth, root.width - 2 * VLCStyle.margin_xsmall)
        }
        contentItem: Item {
            implicitWidth: folderIcon.width + VLCStyle.dp(4, VLCStyle.scale) + locationLabel.implicitWidth
            implicitHeight: locationButton.height
            clip: true
            Widgets.IconLabel {
                id: folderIcon
                width: VLCStyle.dp(28, VLCStyle.scale)
                height: locationButton.height
                text: VLCIcons.folder
                font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
                color: theme.fg.primary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            T.Label {
                id: locationLabel
                x: folderIcon.width + VLCStyle.dp(4, VLCStyle.scale)
                width: Math.max(0, parent.width - x)
                height: locationButton.height
                text: locationButton.text
                font: locationButton.font
                color: theme.fg.primary
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }
        Navigation.parentItem: root
        Navigation.leftItem: sidebarButton
        onClicked: root.backRequested()
    }

    T.Label {
        anchors.horizontalCenter: parent.horizontalCenter
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        // Reserve the furthest visible control on each side, symmetrically,
        // so the title stays window-centered even when it has to disappear.
        readonly property real leftContentRight: Math.max(menuSelector.x + menuSelector.width,
                                                         sidebarButton.visible ? sidebarButton.x + sidebarButton.width : 0,
                                                         locationButton.visible ? locationButton.x + locationButton.width : 0)
        readonly property real rightContentWidth: csdDecorations.active ? csdDecorations.width + csdDecorations.anchors.rightMargin : 0
        width: Math.max(0, Math.min(implicitWidth, root.width - 2 * (Math.max(leftContentRight, rightContentWidth) + VLCStyle.dp(8, VLCStyle.scale))))
        clip: true // A zero-width elided title must not paint over controls.
        text: root.title || qsTr("VLC Media Player")
        font.family: VLCStyle.fontFamily
        font.pixelSize: VLCStyle.fontSize_normal
        font.weight: Font.Normal
        color: theme.fg.primary
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }

    Loader {
        id: csdDecorations
        anchors.right: parent.right
        anchors.rightMargin: VLCStyle.dp(6, VLCStyle.scale)
        y: (VLCStyle.dp(40, VLCStyle.scale) - height) / 2
        height: VLCStyle.dp(24, VLCStyle.scale)
        active: root.showCSD && Qt.platform.os !== "osx"
        sourceComponent: Widgets.CSDWindowButtonSet {
            playerStyle: true
        }
    }

    Menus.Menubar {
        id: menubar
        anchors.left: parent.left
        anchors.right: parent.right
        y: VLCStyle.dp(40, VLCStyle.scale)
        visible: root.showToolbar
        enabled: visible
        onMenuOpenedChanged: root.requestLockUnlockAutoHide(menuOpened)
    }

}
