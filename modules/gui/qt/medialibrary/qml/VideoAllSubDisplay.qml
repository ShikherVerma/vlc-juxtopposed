// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Util
import VLC.Style
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root

    // Keep the existing route and flat folder model available to other callers.
    property alias parentId: folders.parentId
    property var breadcrumbs: []
    property url folderUrl: ""
    property int initialIndex: -1
    property string searchText: ""
    readonly property Item _activeView: foldersView.visible && folders.count > 0 ? foldersView : videosView
    readonly property int currentIndex: _activeView.currentIndex
    readonly property bool isLoading: _activeView.isLoading
    readonly property Item currentItem: _activeView.currentItem
    readonly property int count: _activeView.count
    readonly property bool hasFolder: folders.parentId.isValid()
    readonly property bool videoViewActive: hasFolder && (folders.count === 0 || videos.count > 0)
    readonly property int itemCount: folders.count + (hasFolder ? videos.count : 0)
    signal showList(var model, int reason)

    title: qsTr("Videos")
    hasGridListMode: true
    isSearchable: true
    sortModel: videoViewActive ? [
        { text: qsTr("Title"), criteria: "title" },
        { text: qsTr("Play count"), criteria: "playcount" },
        { text: qsTr("Date added"), criteria: "insertion" }
    ] : [{ text: qsTr("Title"), criteria: "title" }]

    readonly property var sortChoices: [
        { text: qsTr("Unwatched First"), criteria: "playcount", order: Qt.AscendingOrder },
        { text: qsTr("Watched First"), criteria: "playcount", order: Qt.DescendingOrder },
        { text: qsTr("Newest First"), criteria: "insertion", order: Qt.DescendingOrder },
        { text: qsTr("Oldest First"), criteria: "insertion", order: Qt.AscendingOrder },
        { text: qsTr("Title A-Z"), criteria: "title", order: Qt.AscendingOrder },
        { text: qsTr("Title Z-A"), criteria: "title", order: Qt.DescendingOrder }
    ].filter(choice => videoViewActive || choice.criteria === "title")

    function folderRoute(name) {
        return [...root.pagePrefix.slice(0, -1), name]
    }

    function browse(item, reason) {
        if (!item?.id || !item.id.isValid())
            return
        History.push(folderRoute("folder"), {
            parentId: item.id,
            folderUrl: item.mrl.toString(),
            breadcrumbs: [...root.breadcrumbs, { title: item.title, id: item.id, mrl: item.mrl.toString() }]
        }, reason)
    }

    function ancestor(index) {
        if (index < 0) {
            History.push(folderRoute("base"), Qt.MouseFocusReason)
            return
        }
        const item = breadcrumbs[index]
        History.push(folderRoute("folder"), {
            parentId: item.id, folderUrl: item.mrl,
            breadcrumbs: breadcrumbs.slice(0, index + 1)
        }, Qt.MouseFocusReason)
    }

    // MainDisplay's existing asynchronous history-index restoration targets
    // this page, rather than the dormant/empty sibling view.
    function resetFocus() {
        _activeView.initialIndex = initialIndex
        _activeView.resetFocus()
    }

    function setCurrentItemFocus(reason) {
        if (folders.count > 0)
            foldersView.setCurrentItemFocus(reason)
        else
            videosView.setCurrentItemFocus(reason)
    }

    Navigation.cancelAction: function() { History.previous(Qt.BacktabFocusReason) }
    onVideoViewActiveChanged: {
        if (!videoViewActive && sort.criteria !== "title")
            sort.criteria = "title"
    }
    Component.onCompleted: {
        if (!root.sortChoices.some(choice => choice.criteria === root.sort.criteria))
            root.sort.criteria = "title"
        console.debug("Library initial query:", root.searchText, root.search.pattern)
        // PageExt's replaced default header also initializes the shared SearchCtx.
        // Restore the leaf query after all inherited header bindings settle.
        Qt.callLater(() => {
            root.search.pattern = root.searchText
            console.debug("Library restored query:", root.searchText, root.search.pattern)
        })
    }

    // Genuine model readbacks, useful for diagnosing asynchronous indexing.
    function report() {
        console.log("Library folders:", breadcrumbs.map(x => x.title).join("/"),
                    "folders:", folders.count, "videos:", hasFolder ? videos.count : 0,
                    "search:", search.pattern, "sort:", sort.criteria, sort.order,
                    "grid:", MainCtx.gridView)
        folders.getDataFlat(Array.from({length: Math.min(folders.count, 30)}, (_, i) => i), data => {
            console.debug("Library folder rows:", JSON.stringify(data.map(item => ({title: item.title, id: item.id.toString(), mrl: item.mrl.toString()}))))
        })
        if (hasFolder)
            videos.getDataFlat(Array.from({length: Math.min(videos.count, 30)}, (_, i) => i), data => {
                console.debug("Library video rows:", JSON.stringify(data.map(item => ({title: item.title, progress: item.progress, playcount: item.playcount}))))
            })
    }
    Connections { target: folders; function onCountChanged() { root.report() } }
    Connections { target: videos; function onCountChanged() { root.report() } }
    Connections { target: MainCtx; function onGridViewChanged() { root.report() } }
    Connections {
        target: root.sort
        function onCriteriaChanged() {
            if (!root.sortChoices.some(choice => choice.criteria === root.sort.criteria))
                root.sort.criteria = "title"
        }
    }
    Binding { target: root.search; property: "pattern"; value: root.searchText }
    Connections { target: root.search; function onAskShow() { searchInput.forceActiveFocus(Qt.ShortcutFocusReason) } }
    Connections { target: root.sort; function onAskShow() { sortPopup.open() } }

    MLVideoFoldersModel {
        id: folders
        ml: MediaLib
        hierarchical: true
        searchPattern: root.search.pattern
        sortCriteria: "title"
        sortOrder: root.sort.criteria === "title" ? root.sort.order : Qt.AscendingOrder
    }
    MLVideoModel {
        id: videos
        ml: MediaLib
        parentId: folders.parentId
        searchPattern: root.search.pattern
        sortCriteria: root.sort.criteria
        // Only playcount defaults to DESC in the pinned ML. Insertion date
        // uses ordinary direction: Newest descending, Oldest ascending.
        sortOrder: root.sort.criteria === "playcount"
            ? (root.sort.order === Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder)
            : root.sort.order
    }

    readonly property ColorContext colorContext: ColorContext { id: theme; colorSet: ColorContext.View }

    component LibraryMenu: Controls.Menu {
        width: VLCStyle.dp(265, VLCStyle.scale)
        padding: VLCStyle.dp(4, VLCStyle.scale)
        background: Widgets.PopupBackground { radius: VLCStyle.contextMenu_radius; colorContext.palette: VLCStyle.palette }
    }
    component LibraryMenuItem: Controls.MenuItem {
        id: entry
        property string iconText: ""
        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)
        leftPadding: VLCStyle.dp(14, VLCStyle.scale)
        rightPadding: VLCStyle.dp(14, VLCStyle.scale)
        indicator: null
        arrow: null
        readonly property ColorContext colorContext: ColorContext {
            id: menuTheme
            colorSet: ColorContext.ComboBox
            palette: VLCStyle.palette
            hovered: entry.highlighted
            enabled: entry.enabled
        }
        background: Rectangle {
            radius: VLCStyle.control_radius
            color: entry.highlighted || entry.checked ? menuTheme.bg.secondary : "transparent"
        }
        contentItem: RowLayout {
            spacing: VLCStyle.dp(10, VLCStyle.scale)
            Widgets.IconLabel { visible: entry.iconText !== ""; text: entry.iconText; color: menuTheme.fg.primary; font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
            Widgets.MenuLabel { Layout.fillWidth: true; text: entry.text; color: menuTheme.fg.primary; font.pixelSize: VLCStyle.dp(14, VLCStyle.scale); font.weight: Font.Normal }
            Widgets.IconLabel { visible: entry.checked; text: VLCIcons.check; color: menuTheme.accent; font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
        }
    }

    header: Item {
        implicitHeight: VLCStyle.dp(72, VLCStyle.scale)
        RowLayout {
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: VLCStyle.dp(20, VLCStyle.scale); rightMargin: VLCStyle.dp(20, VLCStyle.scale); topMargin: VLCStyle.dp(20, VLCStyle.scale) }
            height: VLCStyle.dp(34, VLCStyle.scale)
            spacing: VLCStyle.dp(8, VLCStyle.scale)
            Widgets.IconToolButton {
                Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                Layout.fillHeight: true
                text: VLCIcons.back
                description: qsTr("Back")
                enabled: !History.previousEmpty
                onClicked: History.previous(Qt.MouseFocusReason)
            }
            Widgets.IconToolButton {
                Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                Layout.fillHeight: true
                text: VLCIcons.back
                rotation: 180
                description: qsTr("Forward")
                enabled: !History.nextEmpty
                onClicked: History.next(Qt.MouseFocusReason)
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: VLCStyle.dp(4, VLCStyle.scale)
                Controls.ToolButton {
                    text: qsTr("Videos")
                    font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                    contentItem: Widgets.MenuLabel { text: parent.text; color: theme.fg.primary; font: parent.font }
                    background: Rectangle { color: parent.hovered ? theme.bg.highlight : "transparent"; radius: VLCStyle.control_radius }
                    onClicked: root.ancestor(-1)
                }
                Repeater {
                    model: root.breadcrumbs
                    RowLayout {
                        required property var modelData
                        required property int index
                        spacing: VLCStyle.dp(4, VLCStyle.scale)
                        Widgets.MenuLabel { text: "›"; color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(16, VLCStyle.scale) }
                        Controls.ToolButton {
                            text: modelData.title
                            font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                            font.weight: index === root.breadcrumbs.length - 1 ? Font.DemiBold : Font.Normal
                            contentItem: Widgets.MenuLabel { text: parent.text; color: theme.fg.primary; font: parent.font; elide: Text.ElideMiddle }
                            background: Rectangle { color: parent.hovered ? theme.bg.highlight : "transparent"; radius: VLCStyle.control_radius }
                            onClicked: root.ancestor(index)
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Widgets.MenuLabel { text: (root.itemCount === 1 ? qsTr("%1 item") : qsTr("%1 items")).arg(root.itemCount); color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(14, VLCStyle.scale); font.weight: Font.Normal }
            Widgets.IconToolButton {
                id: sortButton
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.fillHeight: true
                text: sortPopup.visible ? VLCIcons.sort_state_active : VLCIcons.topbar_sort
                description: qsTr("Sort")
                onClicked: sortPopup.open()
                LibraryMenu {
                    id: sortPopup
                    y: sortButton.height + VLCStyle.dp(8, VLCStyle.scale)
                    Repeater {
                        model: root.sortChoices
                        LibraryMenuItem {
                            required property var modelData
                            text: modelData.text
                            checkable: true
                            checked: root.sort.criteria === modelData.criteria && root.sort.order === modelData.order
                            onTriggered: { root.sort.criteria = modelData.criteria; root.sort.order = modelData.order; root.report() }
                        }
                    }
                }
            }
            Widgets.IconToolButton {
                id: viewButton
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.fillHeight: true
                text: MainCtx.gridView ? VLCIcons.grid : VLCIcons.list
                description: qsTr("View options")
                onClicked: viewPopup.open()
                LibraryMenu {
                    id: viewPopup
                    y: viewButton.height + VLCStyle.dp(8, VLCStyle.scale)
                    LibraryMenuItem { text: qsTr("Grid View"); iconText: VLCIcons.grid; checkable: true; checked: MainCtx.gridView; onTriggered: MainCtx.gridView = true }
                    LibraryMenuItem { text: qsTr("List View"); iconText: VLCIcons.list; checkable: true; checked: !MainCtx.gridView; onTriggered: MainCtx.gridView = false }
                }
            }
            Widgets.TextFieldExt {
                id: searchInput
                Layout.preferredWidth: Math.min(VLCStyle.dp(325, VLCStyle.scale), root.width / 5)
                Layout.fillHeight: true
                placeholderText: qsTr("Search")
                Accessible.name: qsTr("Search current folder")
                leftPadding: VLCStyle.dp(42, VLCStyle.scale)
                bgColor: VLCStyle.searchInputColor(theme.palette?.isDark ?? VLCStyle.palette.isDark)
                text: root.searchText
                onTextEdited: {
                    root.searchText = text
                    History.setViewProperty("searchText", text)
                    console.debug("Library search history:", History.viewPath.join("/"), History.viewProp.searchText)
                }
                Widgets.IconLabel {
                    anchors { left: parent.left; leftMargin: VLCStyle.dp(10, VLCStyle.scale); verticalCenter: parent.verticalCenter }
                    text: VLCIcons.search
                    font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
                    color: theme.fg.secondary
                }
            }
            Widgets.IconToolButton {
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.fillHeight: true
                text: VLCIcons.add
                description: qsTr("Add library folder — Preferences, Media Library, Add, Save")
                onClicked: DialogsProvider.mediaLibraryDialog()
            }
            Widgets.IconToolButton {
                id: moreButton
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.fillHeight: true
                text: VLCIcons.more
                description: qsTr("Folder actions")
                onClicked: morePopup.open()
                LibraryMenu {
                    id: morePopup
                    x: moreButton.width - width
                    y: moreButton.height + VLCStyle.dp(8, VLCStyle.scale)
                    LibraryMenuItem { text: qsTr("Add library folder"); onTriggered: DialogsProvider.mediaLibraryDialog() }
                    LibraryMenuItem {
                        text: qsTr("Rescan")
                        enabled: !root.hasFolder || root.folderUrl.toString() !== ""
                        Accessible.description: enabled ? qsTr("Rescan the current folder, or all entry points at Videos root") : qsTr("The missing folder has no available URL to rescan")
                        onTriggered: folders.rescan(root.folderUrl)
                    }
                    LibraryMenuItem { text: qsTr("Open in file manager"); enabled: root.folderUrl.toString() !== ""; onTriggered: Qt.openUrlExternally(root.folderUrl) }
                    LibraryMenuItem {
                        text: qsTr("Play folder")
                        enabled: root.hasFolder && videos.count > 0
                        Accessible.description: enabled ? qsTr("Play indexed folder members") : qsTr("No indexed videos in the current view")
                        onTriggered: MediaLib.addAndPlay(root.parentId)
                    }
                    LibraryMenuItem {
                        text: qsTr("Enqueue folder")
                        enabled: root.hasFolder && videos.count > 0
                        Accessible.description: enabled ? qsTr("Enqueue indexed folder members") : qsTr("No indexed videos in the current view")
                        onTriggered: MediaLib.addToPlaylist(root.parentId)
                    }
                }
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: VLCStyle.dp(20, VLCStyle.scale); rightMargin: VLCStyle.dp(20, VLCStyle.scale) }
            height: VLCStyle.dp(1, VLCStyle.scale)
            readonly property ColorContext colorContext: ColorContext { id: dividerTheme; colorSet: ColorContext.ComboBox }
            color: dividerTheme.bg.primary
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        VideoAll {
            id: foldersView
            libraryLayout: true
            sortContext: root.sort
            Layout.fillWidth: true
            Layout.fillHeight: !videosView.visible
            Layout.preferredHeight: videosView.visible ? Math.min(root.height / 2, VLCStyle.dp(260, VLCStyle.scale)) : -1
            visible: folders.count > 0 || !root.hasFolder
            model: folders
            initialIndex: root.initialIndex
            sectionProperty: ""
            contextMenu: MLContextMenu { model: folders }
            gridLabels: () => []
            listLabels: () => []
            coverWidth: VLCStyle.dp(316, VLCStyle.scale)
            coverHeight: VLCStyle.dp(178, VLCStyle.scale)
            showDuration: false
            leftPadding: VLCStyle.dp(20, VLCStyle.scale)
            rightPadding: VLCStyle.dp(20, VLCStyle.scale)
            function onAction(indexes) { root.browse(folders.getDataAt(indexes[0]), Qt.TabFocusReason) }
            function onDoubleClick(item) { root.browse(item, Qt.MouseFocusReason) }
        }
        VideoAll {
            id: videosView
            libraryLayout: true
            sortContext: root.sort
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.hasFolder && (videos.count > 0 || folders.count === 0)
            model: videos
            initialIndex: root.initialIndex
            sectionProperty: ""
            contextMenu: MLContextMenu { model: videos; showPlayAsAudioAction: true }
            gridLabels: () => []
            listLabels: () => []
            coverWidth: VLCStyle.dp(316, VLCStyle.scale)
            coverHeight: VLCStyle.dp(178, VLCStyle.scale)
            showDuration: false
            leftPadding: VLCStyle.dp(20, VLCStyle.scale)
            rightPadding: VLCStyle.dp(20, VLCStyle.scale)
            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd
        }
    }
}
