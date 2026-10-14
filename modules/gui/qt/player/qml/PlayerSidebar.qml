/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Playlist
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

FocusScope {
    id: root
    implicitWidth: VLCStyle.dp(400)
    readonly property real minimumWidth: VLCStyle.dp(360)
    readonly property real maximumWidth: VLCStyle.dp(400)
    property int currentTab: 0
    readonly property var listView: queue.listView
    readonly property var view: queue.view
    readonly property var activeView: currentTab === 0 ? queue.view : currentTab === 1 ? bookmarkList : chapterList
    readonly property var bookmarkModel: bookmarkLoader.item
    readonly property bool bookmarksAvailable: bookmarkModel && bookmarkModel.available
    readonly property var model: queue.model
    readonly property var selectionModel: queue.selectionModel
    readonly property var isDropAcceptableFunc: queue.isDropAcceptableFunc
    readonly property var acceptDropFunc: queue.acceptDropFunc
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Queue, Bookmarks and Chapters")
    AccessibleCompat.id: "playerSidebar"

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    function selectTab(index, focusList = false) {
        currentTab = (index + 3) % 3
        if (focusList)
            activeView.forceActiveFocus(Qt.TabFocusReason)
        else
            tabs.itemAt(currentTab).forceActiveFocus(Qt.TabFocusReason)
    }

    function clock(ms, milliseconds = false) {
        const value = Math.max(0, Math.floor(ms))
        const seconds = Math.floor(value / 1000)
        const pad = (n, length) => String(n).padStart(length, "0")
        return pad(Math.floor(seconds / 3600), 2) + ":" + pad(Math.floor(seconds / 60) % 60, 2) + ":" + pad(seconds % 60, 2)
             + (milliseconds ? ":" + pad(value % 1000, 3) : "")
    }

    onVisibleChanged: if (visible) activeView.forceActiveFocus(Qt.OtherFocusReason)

    Rectangle {
        anchors.fill: parent
        color: theme.bg.primary
        radius: VLCStyle.dp(10)
    }

    Loader {
        id: bookmarkLoader
        active: MainCtx.mediaLibraryAvailable
        source: "qrc:///qt/qml/VLC/MediaLibrary/SidebarBookmarkModel.qml"
    }

    // The native video window otherwise gives engine hotkeys priority over mouse-focused controls.
    KeyEventFilter {
        target: MainCtx.intfMainWindow
        enabled: MainCtx.playerView
        Keys.onPressed: event => {
            if (event.key === Qt.Key_B && event.modifiers === Qt.ControlModifier && root.bookmarksAvailable && Player.isStarted && Player.seekable) {
                root.bookmarkModel.add()
                event.accepted = true
            } else if (event.key === Qt.Key_F2 && root.visible && root.currentTab === 1 && bookmarkList.count > 0) {
                bookmarkList.currentItem?.beginRename()
                event.accepted = true
            }
        }
    }

    Rectangle {
        width: parent.width
        height: VLCStyle.dp(50)
        radius: VLCStyle.dp(10)
        color: theme.bg.secondary
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: VLCStyle.dp(10)
            color: parent.color
        }
    }

    Row {
        id: tabRow
        x: VLCStyle.dp(6)
        width: parent.width - VLCStyle.dp(12)
        height: VLCStyle.dp(50)
        spacing: VLCStyle.dp(1)
        Repeater {
            id: tabs
            model: [qsTr("Queue"), qsTr("Bookmarks"), qsTr("Chapters")]
            T.Button {
                id: tab
                required property int index
                required property string modelData
                width: (tabRow.width - tabRow.spacing * 2) / 3
                height: tabRow.height
                text: modelData
                hoverEnabled: true
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.selected: root.currentTab === index
                AccessibleCompat.id: "playerSidebarTab" + index
                onClicked: root.selectTab(index, true)
                background: Rectangle {
                    color: tab.hovered ? tabTheme.bg.primary : "transparent"
                    radius: VLCStyle.dp(4)
                    border.width: tab.visualFocus ? VLCStyle.dp(1) : 0
                    border.color: theme.accent
                }
                readonly property ColorContext colorContext: ColorContext {
                    id: tabTheme
                    colorSet: ColorContext.TabButton
                    hovered: tab.hovered
                    focused: tab.visualFocus
                }
                contentItem: Item {
                  Row {
                    spacing: VLCStyle.dp(8)
                    anchors.centerIn: parent
                    Widgets.IconLabel {
                        width: VLCStyle.dp(20)
                        height: width
                        font.pixelSize: VLCStyle.dp(20)
                        text: index === 0 ? (root.currentTab === 0 ? VLCIcons.queue_two_tone_yes : VLCIcons.queue)
                             : index === 1 ? (root.currentTab === 1 ? VLCIcons.bookmarks_two_tone_yes : VLCIcons.bookmarks)
                             : (root.currentTab === 2 ? VLCIcons.chapters_two_tone_yes : VLCIcons.chapters)
                        color: root.currentTab === index ? theme.accent : theme.fg.secondary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.text
                        font.pixelSize: VLCStyle.dp(12)
                        // The native sheets retain quiet labels even on filled active tabs.
                        color: (root.currentTab === 0 ? index !== 2 : root.currentTab === 1 ? index === 0 : index === 1)
                               ? tabTheme.fg.primary : theme.fg.secondary
                    }
                }
                }
                Rectangle {
                    x: VLCStyle.dp(8)
                    width: parent.width - VLCStyle.dp(16)
                    y: parent.height - VLCStyle.dp(2)
                    height: VLCStyle.dp(2)
                    color: root.currentTab === index ? theme.accent : theme.bg.secondary
                }
                Navigation.parentItem: root
                Navigation.leftAction: () => root.selectTab(index - 1)
                Navigation.rightAction: () => root.selectTab(index + 1)
                Navigation.downItem: root.activeView
                Keys.onPressed: event => tab.Navigation.defaultKeyAction(event)
                Keys.onReturnPressed: root.selectTab(index, true)
            }
        }
    }

    PlaylistPane {
        id: queue
        anchors { top: tabRow.bottom; topMargin: VLCStyle.dp(1); bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: root.currentTab === 0
        sidebarMode: true
        sidebarTab: tabs.itemAt(0)
        useAcrylic: false
        background: Rectangle { color: "transparent" }
        focus: visible
        Navigation.parentItem: root
    }

    RowLayout {
        id: bookmarkHeader
        visible: root.currentTab === 1
        x: VLCStyle.dp(28)
        y: VLCStyle.dp(51)
        width: parent.width - VLCStyle.dp(48)
        height: VLCStyle.dp(64)
        spacing: VLCStyle.dp(12)
        Text {
            Layout.fillWidth: true
            text: qsTr("%1 Bookmarks").arg(bookmarkList.count)
            font.pixelSize: VLCStyle.dp(14)
            color: theme.fg.secondary
        }
        Widgets.IconToolButton {
            text: VLCIcons.remove
            padding: 0
            Layout.preferredWidth: VLCStyle.dp(24)
            description: qsTr("Delete selected bookmark")
            enabled: bookmarkList.count > 0 && bookmarkList.currentIndex >= 0
            onClicked: root.bookmarkModel.removeRow(bookmarkList.currentIndex)
            Navigation.parentItem: root
            Navigation.downItem: bookmarkList
        }
        Widgets.IconToolButton {
            id: addBookmark
            padding: 0
            Layout.preferredWidth: VLCStyle.dp(24)
            text: VLCIcons.add
            description: Qt.platform.os === "osx"
                         ? qsTr("Add bookmark at current time (%1)").arg(KeyHelper.nativeShortcutText("Ctrl+B"))
                         : qsTr("Add bookmark at current time (Ctrl+B)")
            enabled: root.bookmarksAvailable && Player.isStarted && Player.time.valid() && Player.seekable
            onClicked: root.bookmarkModel.add()
            Navigation.parentItem: root
            Navigation.downItem: bookmarkList
            AccessibleCompat.id: "playerSidebarAddBookmark"
        }
    }

    Widgets.ListViewExt {
        id: bookmarkList
        visible: root.currentTab === 1
        anchors { top: bookmarkHeader.bottom; topMargin: VLCStyle.dp(1); left: parent.left; leftMargin: VLCStyle.dp(20); right: parent.right; rightMargin: VLCStyle.dp(20); bottom: parent.bottom }
        spacing: VLCStyle.dp(12)
        clip: true
        model: root.bookmarkModel
        selectionModel: ListSelectionModel { model: root.bookmarkModel }
        Navigation.parentItem: root
        Navigation.upItem: tabs.itemAt(1)
        onActionAtIndex: index => { if (root.bookmarkModel) root.bookmarkModel.select(root.bookmarkModel.index(index, 0)) }
        Keys.onDeletePressed: if (root.bookmarkModel) root.bookmarkModel.removeRow(currentIndex)
        delegate: T.ItemDelegate {
            id: bookmarkRow
            required property int index
            required property string name
            required property double timeMs
            property bool editing: false
            width: bookmarkList.width
            height: VLCStyle.dp(32)
            hoverEnabled: true
            Accessible.name: name + " " + root.clock(timeMs, true)
            padding: 0
            rightPadding: VLCStyle.dp(4)
            function beginRename() {
                editing = true
                editor.text = name
                Qt.callLater(() => {
                    if (!editing)
                        return
                    editor.forceActiveFocus(Qt.OtherFocusReason)
                    editor.selectAll()
                })
            }
            ListView.onPooled: editing = false
            onClicked: {
                bookmarkList.currentIndex = index
                root.bookmarkModel.select(root.bookmarkModel.index(index, 0))
                forceActiveFocus(Qt.MouseFocusReason)
            }
            background: Rectangle {
                radius: VLCStyle.dp(4)
                color: bookmarkRow.hovered || bookmarkRow.activeFocus ? rowTheme.bg.highlight : "transparent"
                border.width: bookmarkRow.visualFocus ? VLCStyle.dp(1) : 0
                border.color: theme.accent
            }
            readonly property ColorContext colorContext: ColorContext { id: rowTheme; colorSet: ColorContext.Item }
            contentItem: RowLayout {
                spacing: VLCStyle.dp(10)
                Text {
                    visible: !bookmarkRow.editing
                    Layout.fillWidth: true
                    text: bookmarkRow.name
                    font.pixelSize: VLCStyle.dp(14)
                    color: theme.fg.primary
                    elide: Text.ElideRight
                }
                Widgets.TextFieldExt {
                    id: editor
                    visible: bookmarkRow.editing
                    Layout.fillWidth: true
                    onAccepted: {
                        if (root.bookmarkModel.rename(bookmarkRow.index, text)) {
                            bookmarkRow.editing = false
                            bookmarkRow.forceActiveFocus(Qt.TabFocusReason)
                        }
                    }
                    Keys.onEscapePressed: {
                        bookmarkRow.editing = false
                        bookmarkRow.forceActiveFocus(Qt.TabFocusReason)
                    }
                }
                Text {
                    text: root.clock(bookmarkRow.timeMs, true)
                    font.pixelSize: VLCStyle.dp(14)
                    color: theme.fg.secondary
                }
                Widgets.IconToolButton {
                    visible: bookmarkRow.hovered || bookmarkRow.activeFocus
                    text: VLCIcons.edit
                    description: qsTr("Rename bookmark (F2)")
                    onClicked: bookmarkRow.beginRename()
                    Navigation.parentItem: root
                }
            }
        }
        Text {
            anchors.centerIn: parent
            width: parent.width - VLCStyle.dp(16)
            visible: bookmarkList.count === 0
            text: !Player.isStarted ? qsTr("Play a media file to use bookmarks.")
                  : !root.bookmarksAvailable ? qsTr("Bookmarks require this media to be in the media library.")
                  : qsTr("No bookmarks yet. Add one at the current time.")
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: VLCStyle.dp(14)
            color: theme.fg.secondary
        }
    }

    Widgets.ListViewExt {
        id: chapterList
        visible: root.currentTab === 2
        anchors { top: tabRow.bottom; topMargin: VLCStyle.dp(21); left: parent.left; leftMargin: VLCStyle.dp(20); right: parent.right; rightMargin: VLCStyle.dp(20); bottom: parent.bottom }
        spacing: VLCStyle.dp(12)
        clip: true
        model: Player.chapters
        selectionModel: ListSelectionModel { model: Player.chapters }
        Navigation.parentItem: root
        Navigation.upItem: tabs.itemAt(2)
        onActionAtIndex: index => Player.chapters.selectChapter(index)
        delegate: T.ItemDelegate {
            id: chapterRow
            required property int index
            required property var model
            readonly property string chapterName: model.display
            required checked
            required property var time
            width: chapterList.width
            height: VLCStyle.dp(32)
            hoverEnabled: true
            padding: VLCStyle.dp(4)
            Accessible.name: (index + 1) + " " + chapterName + " " + root.clock(time.toSeconds() * 1000)
            Accessible.selected: checked
            onClicked: {
                chapterList.currentIndex = index
                Player.chapters.selectChapter(index)
                forceActiveFocus(Qt.MouseFocusReason)
            }
            background: Rectangle {
                radius: VLCStyle.dp(4)
                color: chapterRow.checked || chapterRow.hovered || chapterRow.activeFocus ? rowTheme.bg.highlight : "transparent"
                border.width: chapterRow.visualFocus ? VLCStyle.dp(1) : 0
                border.color: theme.accent
                Rectangle {
                    visible: chapterRow.checked
                    width: VLCStyle.dp(2)
                    height: VLCStyle.dp(16)
                    anchors.verticalCenter: parent.verticalCenter
                    color: theme.accent
                }
            }
            readonly property ColorContext colorContext: ColorContext { id: rowTheme; colorSet: ColorContext.Item }
            contentItem: RowLayout {
                spacing: VLCStyle.dp(10)
                Text {
                    Layout.preferredWidth: VLCStyle.dp(34)
                    text: chapterRow.index + 1
                    font.pixelSize: VLCStyle.dp(14)
                    font.weight: Font.Medium
                    color: theme.accent
                }
                Text {
                    Layout.fillWidth: true
                    text: chapterRow.chapterName
                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.dp(14)
                    color: theme.fg.primary
                }
                Text {
                    text: root.clock(chapterRow.time.toSeconds() * 1000)
                    font.pixelSize: VLCStyle.dp(14)
                    color: theme.fg.secondary
                }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: chapterList.count === 0
            text: qsTr("This media has no chapters.")
            font.pixelSize: VLCStyle.dp(14)
            color: theme.fg.secondary
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Tab && (event.modifiers & Qt.ControlModifier)) {
            root.selectTab(currentTab + (event.modifiers & Qt.ShiftModifier ? -1 : 1), true)
            event.accepted = true
        } else {
            root.Navigation.defaultKeyAction(event)
        }
    }
}
