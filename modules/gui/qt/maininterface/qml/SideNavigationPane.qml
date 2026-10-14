/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import QtQml.Models

import VLC.Style
import VLC.Widgets as Widgets

import VLC.MainInterface
import VLC.Dialogs
import VLC.Util

T.Pane {
    id: root

    signal itemClicked(uri : var, item : Item)

    //safe area margins for delegate content,
    //so the background of the delegates fills the available space
    //but the content remains within the safe area
    property int safeAreaLeftMargin: 0
    property int safeAreaRightMargin: 0

    property bool delegateShowText: true

    property alias model: navigationModel

    readonly property int minimumWidth: VLCStyle.dp(217, VLCStyle.scale) + safeAreaLeftMargin + safeAreaRightMargin

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    topPadding: VLCStyle.dp(delegateShowText ? 20 : 8, VLCStyle.scale)
    bottomPadding: VLCStyle.dp(20, VLCStyle.scale)
    leftPadding: VLCStyle.dp(delegateShowText ? 24 : 10, VLCStyle.scale)
    rightPadding: VLCStyle.dp(delegateShowText ? 24 : 10, VLCStyle.scale)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Accessible.name: qsTr("Sidebar")

    NavigationModel {
        id: navigationModel
        hasMedialib: MainCtx.mediaLibraryAvailable
    }

    // Longest route wins (Playlists aliases existing Video/Music paths). Also
    // recognize children while their parent is collapsed or history changes.
    function selectedRow() {
        let best = -1
        let length = 0
        for (let i = 0; i < navigationModel.rowCount(); ++i) {
            const index = navigationModel.index(i, 0)
            const uri = navigationModel.data(index, NavigationModel.URI)
            const children = navigationModel.data(index, NavigationModel.CHILDREN)
            for (const route of [uri, ...children.map(child => child.uri)]) {
                if (route && route.length >= length && History.match(History.viewPath, route)) {
                    best = i
                    length = route.length
                }
            }
        }
        return best
    }

    property int selectedIndex: -1
    property int selectedParentIndex: -1

    function synchronizeSelection() {
        if (!delegateShowText) {
            for (let i = 0; i < navigationModel.rowCount(); ++i) {
                const index = navigationModel.index(i, 0)
                if (navigationModel.data(index, NavigationModel.DEPTH) === 0 && navigationModel.data(index, NavigationModel.EXPANDED))
                    navigationModel.setData(index, false, NavigationModel.EXPANDED)
            }
        }
        const row = selectedRow()
        if (row < 0)
            return
        const index = navigationModel.index(row, 0)
        if (delegateShowText && !navigationModel.data(index, NavigationModel.EXPANDED))
            navigationModel.setData(index, true, NavigationModel.EXPANDED)
        selectedIndex = selectedRow()
        selectedParentIndex = selectedIndex
        while (selectedParentIndex > 0 && navigationModel.data(navigationModel.index(selectedParentIndex, 0), NavigationModel.DEPTH) > 0)
            --selectedParentIndex
        listView.currentIndex = selectedIndex
    }

    onDelegateShowTextChanged: Qt.callLater(synchronizeSelection)
    Component.onCompleted: {
        if (!MainCtx.mediaLibraryAvailable)
            console.info("Library navigation: Videos, Music, Playlists, Recent and folder-add unavailable without the media-library module")
        Qt.callLater(synchronizeSelection)
    }
    Connections {
        target: History
        function onViewPathChanged() { root.synchronizeSelection() }
    }

    background: Rectangle {
        color: viewTheme.bg.primary
        ColorContext { id: viewTheme; colorSet: ColorContext.View }
        Rectangle {
            anchors.fill: parent
            anchors.margins: root.delegateShowText ? VLCStyle.dp(8, VLCStyle.scale) : 0
            radius: root.delegateShowText ? VLCStyle.dp(6, VLCStyle.scale) : 0
            color: VLCStyle.popupTint(theme.palette.isDark)
        }
    }

    contentItem:  Item {
        id: contentItemId

        //don't use Layout as top item as items put as children of the Control
        //would be parented (and layouted) to the Layout
        ColumnLayout {
            anchors.fill: parent
            spacing: VLCStyle.dp(14, VLCStyle.scale)

            Widgets.ListViewExt {
                id: listView

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: !fadingEdge.implicitClipping && (height < contentHeight)
                focus: true

                model: navigationModel
                spacing: VLCStyle.dp(14, VLCStyle.scale)
                onActionAtIndex: (index) => {
                    const item = itemAtIndex(index)
                    if (item && item.visible && item.enabled)
                        item.clicked()
                }

                Binding {
                    target: listView.T.ScrollBar.vertical
                    property: "policy"
                    when: listView.T.ScrollBar.vertical?.size > (1.0 - Number.EPSILON)
                    value: T.ScrollBar.AlwaysOff
                }

                colorContext.colorSet: ColorContext.Window

                fadingEdge.backgroundColor: VLCStyle.popupTint(theme.palette.isDark)

                component DelegateAnimation : NumberAnimation {
                    easing.type: Easing.InOutSine
                    duration: VLCStyle.duration_long
                }

                reuseItems: false // Not relevant here, we disable it because of QTBUG-131106 when transition is involved.

                // As the docs note, `ListView` transitions do not seem to handle height animation. For that reason,
                // we animate the height directly in the delegate (Qt's recommended workaround). Because of this, we
                // do not need to have `addDisplaced` transition here, but we can have `removeDisplaced` here:
                removeDisplaced: Transition {
                    DelegateAnimation { property: "y" }
                }

                property bool readyForAnimations: false

                onModelChanged: {
                    if (!listView.readyForAnimations)
                        Qt.callLater(() => { listView.readyForAnimations = true })
                }

                Navigation.parentItem: root
                Navigation.downItem: preferenceButton

                delegate: SideNavigationDelegate {
                    id: delegate

                    width: ListView.view.contentWidth
                    height: preferredHeight

                    property real preferredHeight: shouldShow ? VLCStyle.dp(32, VLCStyle.scale) : 0.0

                    leftPadding: root.safeAreaLeftMargin + (depth > 0 ? VLCStyle.dp(38, VLCStyle.scale) : 0)
                    rightPadding: root.safeAreaRightMargin

                    leftInset: root.safeAreaLeftMargin + implicitLeftInset
                    rightInset: root.safeAreaRightMargin + implicitRightInset

                    iconTxt: icon_alias
                    text: title
                    navigationDepth: depth
                    AccessibleCompat.id: {
                        if (depth > 0)
                            return "libraryNav-" + uri.join("-")
                        const ids = { "home": "homeNavBar", "video": "videoNavBar", "music": "musicNavBar",
                                      "network": "browseNavBar", "playlists": "playlistsNavBar", "recent": "recentNavBar", "library-add": "addItemNavBar" }
                        return uri[0] === "video" && uri[1] === "playlists" ? "playlistsNavBar" : (ids[uri[0]] || "")
                    }
                    Accessible.description: uri[0] === "library-add"
                                            ? qsTr("Add library folders in Preferences, Media Library, Add")
                                            : ""

                    showText: root.delegateShowText

                    required property string title
                    required property var uri
                    required property int depth
                    // TODO: Qt 6.2 has property `icon` marked final, so we can't override it:
                    required property string icon_alias
                    required property bool expandable
                    required property bool expanded

                    required property int index

                    visible: (height > 0.0)

                    property bool shouldShow: showText || depth === 0

                    onShouldShowChanged: {
                        if (listView.readyForAnimations)
                            heightBehavior.enabled = true // At this point, it can keep being enabled for good.
                    }

                    highlighted: root.selectedIndex === index || root.selectedParentIndex === index
                    checked: highlighted

                    onClicked: {
                        // History can retract this delegate synchronously when
                        // an alias changes sections. Capture the live view first.
                        const view = ListView.view
                        const reason = focusReason
                        itemClicked(uri, this)
                        Qt.callLater(() => { if (view) view.setCurrentItemFocus(reason) })
                    }

                    Behavior on height {
                        id: heightBehavior

                        enabled: false

                        DelegateAnimation {
                            onRunningChanged: {
                                if (running) {
                                    delegate.clip = true
                                } else {
                                    delegate.clip = false
                                }
                            }
                        }
                    }

                    ListView.onAdd: {
                        if (listView.readyForAnimations && delegate.showText) {
                            heightBehavior.enabled = false
                            delegate.height = 0.0
                            heightBehavior.enabled = true
                            delegate.height = Qt.binding(() => delegate.preferredHeight)
                            heightBehavior.enabled = false
                        }
                    }
                }
            }

            Loader {
                id: loaderProgress

                Layout.fillWidth: true

                active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)
                visible: active

                source: "qrc:///qt/qml/VLC/MediaLibrary/ScanProgressBar.qml"

                onLoaded: {
                    if (item.background) {
                        item.background.opacity = 0.6
                        item.background.radius = Qt.binding(() => VLCStyle.dp(4, VLCStyle.scale))
                    }

                    item.leftPadding = Qt.binding(function() { return root.safeAreaLeftMargin + VLCStyle.margin_small })
                    item.rightPadding = Qt.binding(function() { return root.safeAreaRightMargin + VLCStyle.margin_small })

                    item.leftInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaLeftMargin })
                    item.rightInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaRightMargin })
                }
            }

            SideNavigationDelegate {
                id: preferenceButton

                AccessibleCompat.id: "preferencesButton"

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

                leftPadding: root.safeAreaLeftMargin
                rightPadding: root.safeAreaRightMargin

                leftInset: root.safeAreaLeftMargin + implicitLeftInset
                rightInset: root.safeAreaRightMargin + implicitRightInset

                iconTxt: VLCIcons.settings
                text: qsTr("Preferences")

                checked: DialogsProvider.prefsDialogVisible

                showText: root.delegateShowText

                onClicked: DialogsProvider.prefsDialog()

                //SideNavigationDelegate is an ItemDelegate and has NoFocus by default
                focusPolicy: Qt.StrongFocus

                Navigation.parentItem: root
                Navigation.upItem: listView

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (e) => Navigation.defaultKeyAction(e)
                Keys.onReleased: (e) => Navigation.defaultKeyReleaseAction(e)
            }

        }
    }
}
