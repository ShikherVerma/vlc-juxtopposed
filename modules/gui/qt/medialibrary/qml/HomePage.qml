/****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 ****************************************************************************/
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MediaLibrary
import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

Widgets.PageExt {
    id: root

    title: qsTr("Home")
    hasGridListMode: false
    isSearchable: true
    sortModel: [] // Home has a fixed history/frequency order, not an alphabetic sort.

    signal seeAllButtonClicked(string name, int reason)

    readonly property real unit: VLCStyle.scale
    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }
    readonly property ColorContext glyphTheme: ColorContext { colorSet: ColorContext.ToolButton }
    readonly property ColorContext searchPlaceholderTheme: ColorContext {
        colorSet: ColorContext.TextField
        enabled: false
    }
    property int activeSection: 0
    readonly property var sections: [continueRow, audioRow, folderRow, playlistRow, artistRow, streamRow]

    function positionContentAtBeginning() { flickable.contentY = 0 }
    function setCurrentItemFocus(reason) { sections[activeSection].setCurrentItemFocus(reason) }
    function focusSection(index, reason) {
        if (index < 0) {
            root.header.forceActiveFocus(reason)
            return
        }
        if (index >= sections.length)
            return
        activeSection = index
        sections[index].setCurrentItemFocus(reason)
    }
    function reveal(item) { Helpers.positionFlickableToContainItem(flickable, item) }
    function activate(section, data) {
        // Collections open the genuine existing leaf views. Media and streams
        // play through the same MediaLib API as the shared card delegates.
        console.debug("Home activation:", section, data.id.toString(), data.title ?? data.url)
        if (section === 2) {
            History.push(["home", "folder"], { parentId: data.id, title: data.title }, Qt.OtherFocusReason)
        } else if (section === 3) {
            History.push(["home", "playlist"], { parentId: data.id, title: data.title }, Qt.OtherFocusReason)
        } else if (section === 4) {
            History.push(["home", "artist"], { artistId: data.id }, Qt.OtherFocusReason)
        } else {
            MediaLib.addAndPlay(data.id)
            MainCtx.playerView = true
        }
    }

    // Native 6261:15118: 20px inset, 34px toolbar at y60, divider y111.
    header: T.ToolBar {
        id: toolbar
        height: 72 * root.unit
        readonly property ColorContext colorContext: root.colorContext
        Navigation.parentItem: root
        Navigation.downAction: () => root.setCurrentItemFocus(Qt.TabFocusReason)
        background: Rectangle { color: theme.bg.primary }
        Widgets.NavigableRow {
            id: headerRow
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 * root.unit }
            height: 34 * root.unit
            spacing: 10 * root.unit
            focus: true
            Navigation.parentItem: toolbar
            Widgets.IconToolButton {
                width: 24 * root.unit; height: parent.height
                text: VLCIcons.back
                description: qsTr("Back")
                enabled: !History.previousEmpty
                onClicked: History.previous(Qt.MouseFocusReason)
            }
            Widgets.IconToolButton {
                width: 24 * root.unit; height: parent.height
                text: VLCIcons.arrow_direction_right
                description: qsTr("Forward")
                enabled: false
                Component.onCompleted: console.info("Home: Forward is disabled; retained History supports Back only.")
            }
            Text {
                width: Math.max(0, headerRow.width - x - searchField.width - 10 * root.unit)
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                text: root.title
                Navigation.navigable: false
                color: glyphTheme.fg.primary
                font.pixelSize: 16 * root.unit
                font.weight: Font.Normal
            }
            Widgets.TextFieldExt {
                id: searchField
                width: Math.min(325 * root.unit, headerRow.width * 0.4)
                height: 34 * root.unit
                leftPadding: 42 * root.unit
                rightPadding: 10 * root.unit
                placeholderText: qsTr("Search")
                placeholderTextColor: root.searchPlaceholderTheme.fg.primary
                Accessible.name: qsTr("Search Home")
                Accessible.searchEdit: true
                bgColor: VLCStyle.searchInputColor((theme.palette?.isDark ?? VLCStyle.palette.isDark))
                onActiveFocusChanged: MainCtx.useGlobalShortcuts = !activeFocus
                Component.onDestruction: if (activeFocus) MainCtx.useGlobalShortcuts = true
                Navigation.parentItem: headerRow
                Navigation.downAction: () => root.setCurrentItemFocus(Qt.TabFocusReason)
                Binding { target: root.search; property: "pattern"; value: searchField.text }
                Connections { target: root.search; function onAskShow() { searchField.forceActiveFocus(Qt.ShortcutFocusReason) } }
                Widgets.IconLabel {
                    x: 10 * root.unit; anchors.verticalCenter: parent.verticalCenter
                    text: VLCIcons.search; font.pixelSize: 24 * root.unit; color: theme.fg.secondary
                }
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 20 * root.unit; rightMargin: 20 * root.unit }
            height: root.unit
            color: (theme.palette?.isDark ?? VLCStyle.palette.isDark) ? "#191919" : "#d5d5d5" // named BG/A4 (light A3)
        }
    }

    contentItem: Flickable {
        id: flickable
        clip: true
        contentWidth: width
        contentHeight: rows.height + 16 * root.unit
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        T.ScrollBar.vertical: Widgets.ScrollBarExt {}
        Column {
            id: rows
            width: parent.width
            spacing: 17 * root.unit
            HomeSection {
                id: continueRow
                sectionIndex: 0
                heading: qsTr("Continue Watching")
                emptyText: qsTr("No unfinished videos. Play a video from your library to continue here.")
                video: true
                rowModel: MLHomeModel { ml: MediaLib; section: MLHomeModel.ContinueWatching; searchPattern: root.search.pattern }
            }
            HomeSection {
                id: audioRow
                sectionIndex: 1
                heading: qsTr("Listen Again")
                emptyText: qsTr("No recently played music. Listen to a library song to find it here.")
                rowModel: MLHomeModel { ml: MediaLib; section: MLHomeModel.ListenAgain; searchPattern: root.search.pattern }
            }
            HomeSection {
                id: folderRow
                sectionIndex: 2
                heading: qsTr("Frequently Visited Folders")
                emptyText: qsTr("No played library folders yet.")
                rowModel: MLHomeModel { ml: MediaLib; section: MLHomeModel.Folders; searchPattern: root.search.pattern }
            }
            HomeSection {
                id: playlistRow
                sectionIndex: 3
                heading: qsTr("Frequently Visited Playlists")
                emptyText: qsTr("No saved playlists with played media yet.")
                rowModel: MLHomeModel { ml: MediaLib; section: MLHomeModel.Playlists; searchPattern: root.search.pattern }
            }
            HomeSection {
                id: artistRow
                sectionIndex: 4
                heading: qsTr("Frequently Listened Artists")
                emptyText: qsTr("No listened library artists yet.")
                rowModel: MLHomeModel { ml: MediaLib; section: MLHomeModel.Artists; searchPattern: root.search.pattern }
            }
            HomeSection {
                id: streamRow
                sectionIndex: 5
                heading: qsTr("Latest Streams")
                emptyText: qsTr("No recent network streams. Open a network stream to find it here.")
                rowModel: MLUrlModel {
                    ml: MediaLib
                    sortOrder: Qt.DescendingOrder
                    searchPattern: root.search.pattern
                }
            }
        }
    }

    component HomeSection: FocusScope {
        id: section
        required property int sectionIndex
        required property string heading
        required property string emptyText
        required property var rowModel
        property bool video: false
        readonly property real coverWidth: (video ? 316 : 200) * root.unit
        readonly property real coverHeight: (video ? 177.75 : 200) * root.unit
        width: rows.width
        // Native section containers include 20px top inset and 40px trailing
        // space: video 320.75, music 372, collections 343.
        height: rowModel.count > 0 ? (video ? 320.75 : sectionIndex === 1 ? 372 : 343) * root.unit : Math.max(128 * root.unit, emptyMessage.y + emptyMessage.implicitHeight + 20 * root.unit)
        focus: sectionIndex === 0
        Navigation.parentItem: root
        Navigation.upAction: () => root.focusSection(sectionIndex - 1, Qt.BacktabFocusReason)
        Navigation.downAction: () => root.focusSection(sectionIndex + 1, Qt.TabFocusReason)
        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => section.Navigation.defaultKeyAction(event)
        Accessible.role: Accessible.Section
        Accessible.name: heading
        AccessibleCompat.id: "homeSection" + sectionIndex

        Connections {
            target: section.rowModel
            function onLoadingChanged() {
                if (!section.rowModel.loading) {
                    if (section.activeFocus && section.rowModel.count > 0)
                        Qt.callLater(() => { if (section) section.setCurrentItemFocus(Qt.TabFocusReason) })
                    console.debug("Home section:", section.sectionIndex, "count:", section.rowModel.count)
                    const indexes = []
                    for (let i = 0; i < Math.min(section.rowModel.count, 10); ++i) indexes.push(i)
                    section.rowModel.getDataFlat(indexes, (items) => {
                        for (let i = 0; i < items.length; ++i) {
                            const item = items[i]
                            if (!item?.id) continue
                            console.debug("Home ML row:", section.sectionIndex, i, item.id.toString(),
                                          item.title ?? item.url, "progress:", item.progress ?? 0,
                                          "plays:", item.home_playcount ?? 0, "recent:", item.home_last_played ?? item.last_played_date)
                        }
                    })
                }
            }
        }

        function setCurrentItemFocus(reason) {
            if (list.count > 0) {
                if (list.currentIndex < 0) list.currentIndex = 0
                list.positionViewAtIndex(list.currentIndex, ListView.Contain)
                list.currentItem?.forceActiveFocus(reason)
            } else {
                section.forceActiveFocus(reason)
            }
            root.activeSection = sectionIndex
            root.reveal(section)
        }
        onActiveFocusChanged: if (activeFocus) {
            root.activeSection = sectionIndex
            root.reveal(section)
            if (list.count > 0) section.setCurrentItemFocus(Qt.TabFocusReason)
        }

        Text {
            x: 20 * root.unit; y: 36 * root.unit
            width: parent.width - 40 * root.unit
            height: 24 * root.unit
            text: section.heading
            color: theme.fg.primary
            font.pixelSize: 20 * root.unit
            font.weight: Font.Normal
            elide: Text.ElideRight
        }
        Text {
            id: emptyMessage
            x: 20 * root.unit; y: 80 * root.unit
            width: parent.width - 40 * root.unit
            visible: section.rowModel.count === 0
            text: section.rowModel.loading ? qsTr("Loading…") : root.search.pattern.length > 0 ? qsTr("No matching items.") : section.emptyText
            color: theme.fg.secondary
            font.pixelSize: 16 * root.unit
            wrapMode: Text.WordWrap
        }
        Rectangle {
            visible: section.activeFocus && section.rowModel.count === 0
            x: 16 * root.unit; y: 32 * root.unit
            width: parent.width - 32 * root.unit; height: 84 * root.unit
            color: "transparent"; radius: 6 * root.unit
            border.color: theme.accent
        }
        ListView {
            id: list
            x: 20 * root.unit; y: 80 * root.unit
            width: parent.width - 40 * root.unit
            height: section.coverHeight + (section.sectionIndex === 1 ? 68 : 39) * root.unit + 12 * root.unit
            orientation: ListView.Horizontal
            model: section.rowModel
            spacing: 20 * root.unit
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            T.ScrollBar.horizontal: Widgets.ScrollBarExt {}
            delegate: Widgets.GridItem {
                id: card
                required property int index
                required property var model
                view: list
                width: section.coverWidth
                pictureWidth: section.coverWidth
                pictureHeight: section.coverHeight
                pictureRadius: 12 * root.unit
                activateOnSingleClick: true
                titleFont.pixelSize: 16 * root.unit
                titleFont.weight: Font.Normal
                subtitleFont.pixelSize: 16 * root.unit
                subtitleFont.weight: Font.Normal
                titleTopMargin: 20 * root.unit
                subtitleTopMargin: 10 * root.unit
                title: model.title ?? model.url ?? ""
                subtitle: section.sectionIndex === 1 ? model.subtitle ?? "" : ""
                image: model.small_cover ?? ""
                fallbackImage: section.video ? VLCStyle.noArtVideoCover : section.sectionIndex === 4 ? VLCStyle.noArtArtistCover : VLCStyle.noArtAlbumCover
                fillMode: Image.PreserveAspectCrop
                selectedShadow.visible: false
                unselectedShadow.visible: false
                Navigation.parentItem: section
                Navigation.leftAction: () => {
                    if (index > 0) {
                        list.currentIndex = index - 1
                        section.setCurrentItemFocus(Qt.BacktabFocusReason)
                    } else section.Navigation.defaultNavigationLeft()
                }
                Navigation.rightAction: () => {
                    if (index + 1 < list.count) {
                        list.currentIndex = index + 1
                        section.setCurrentItemFocus(Qt.TabFocusReason)
                    } else section.Navigation.defaultNavigationRight()
                }
                Navigation.upAction: () => root.focusSection(section.sectionIndex - 1, Qt.BacktabFocusReason)
                Navigation.downAction: () => root.focusSection(section.sectionIndex + 1, Qt.TabFocusReason)
                Keys.onReturnPressed: root.activate(section.sectionIndex, model)
                Keys.onEnterPressed: root.activate(section.sectionIndex, model)
                Keys.onSpacePressed: root.activate(section.sectionIndex, model)
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => card.Navigation.defaultKeyAction(event)
                Component.onCompleted: {
                    if (index === 0) Qt.callLater(() => {
                        if (!card || !section) return
                        const origin = card.mediaCover.mapToItem(null, 0, 0)
                        console.debug("Home card geometry:", section.sectionIndex, origin.x, origin.y,
                                      card.mediaCover.width, card.mediaCover.height, card.pictureRadius)
                    })
                }
                onActiveFocusChanged: if (activeFocus) {
                    list.currentIndex = index
                    root.activeSection = section.sectionIndex
                    list.positionViewAtIndex(index, ListView.Contain)
                    root.reveal(section)
                    console.debug("Home focus:", section.sectionIndex, index, model.id.toString(), model.title ?? model.url)
                }
                onItemClicked: list.currentIndex = index
                onItemDoubleClicked: root.activate(section.sectionIndex, model)
                onPlayClicked: root.activate(section.sectionIndex, model)
                // The native folder tile is a plain A2 square with the authored
                // folder glyph, not a generated video collage.
                pictureOverlay: Rectangle {
                    visible: section.sectionIndex === 2 || ((section.sectionIndex === 3 || section.sectionIndex === 5) && card.image.toString() === "")
                    color: (theme.palette?.isDark ?? VLCStyle.palette.isDark) ? "#1f1f1f" : "#dddddd" // named BG/A2
                    radius: 12 * root.unit
                    Widgets.IconLabel { anchors.centerIn: parent; text: section.sectionIndex === 2 ? VLCIcons.folder : section.sectionIndex === 3 ? VLCIcons.media_playlist : VLCIcons.topbar_network; font.pixelSize: 48 * root.unit; color: glyphTheme.fg.primary }
                }
                Rectangle {
                    parent: card.mediaCover
                    anchors.fill: parent
                    visible: card.visualFocus
                    color: "transparent"
                    radius: card.pictureRadius
                    border.width: root.unit
                    border.color: theme.accent
                    z: 2
                }
                Widgets.VideoProgressBar {
                    parent: card.mediaCover
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    visible: section.video && (model.progress ?? 0) > 0
                    radius: card.pictureRadius
                    trackColor: "transparent"
                    value: Helpers.clamp(model.progress ?? 0, 0, 1)
                }
            }
        }
    }
}
