import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Style
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root
    property var model: null
    property var breadcrumbs: []
    property string searchText: ""
    property string emptyText: qsTr("No items in this view. Add a folder using Add item.")
    property string addDescription: ""
    property bool playlists: false
    property bool collections: false
    property int initialIndex: 0
    readonly property bool isLoading: root.model?.loading ?? true
    readonly property int count: root.model?.count ?? 0
    readonly property Item currentItem: MainCtx.gridView ? grid.currentItem : list.currentItem
    readonly property int currentIndex: MainCtx.gridView ? grid.currentIndex : list.currentIndex
    function resetFocus() {
        if (root.count <= 0) return
        const view = MainCtx.gridView ? grid : list
        view.currentIndex = Math.min(Math.max(0, root.initialIndex), root.count - 1)
        view.positionViewAtIndex(view.currentIndex, GridView.Contain)
    }
    onCountChanged: Qt.callLater(root.resetFocus)
    signal activated(var item)
    signal addRequested()
    isSearchable: true
    hasGridListMode: true
    readonly property ColorContext colorContext: ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.View }
    Binding { target: root.search; property: "pattern"; value: root.searchText }
    Component.onCompleted: Qt.callLater(() => {
        root.search.pattern = root.searchText
        console.debug("Library page panels:", MainCtx.playqueuePanel.visible, MainCtx.navigationPanel.visible)
    })
    function itemTitle(item) { return item.title || item.name || qsTr("Untitled") }
    function activate(item) { if (!item?.id || !item.id.isValid()) return; if (root.collections || root.playlists) root.activated(item); else MediaLib.addAndPlay(item.id) }
    function setCurrentItemFocus(reason) { (MainCtx.gridView ? grid : list).forceActiveFocus(reason) }
    header: Widgets.LibraryPageHeader {
        breadcrumbs: root.breadcrumbs
        count: root.model?.count ?? 0
        search: root.search
        sort: root.sort
        searchText: root.searchText
        addDescription: root.addDescription
        onAddRequested: root.addRequested()
        onQueryEdited: query => { root.searchText = query; History.setViewProperty("searchText", query) }
    }
    Connections {
        target: root.model
        function onCountChanged() { console.debug("Library collection:", root.title, "count:", root.model.count, "search:", root.search.pattern) }
    }
    GridView {
        id: grid
        anchors { fill: parent; margins: VLCStyle.dp(20, VLCStyle.scale) }
        clip: true
        visible: MainCtx.gridView
        focus: visible
        model: root.model
        cellWidth: VLCStyle.dp(root.playlists ? 336 : 220, VLCStyle.scale)
        cellHeight: VLCStyle.dp(root.playlists ? 72 : 285, VLCStyle.scale)
        currentIndex: root.initialIndex
        header: root.playlists ? createTile : null
        Keys.onReturnPressed: root.activate(root.model.getDataAt(currentIndex))
        Keys.onSpacePressed: root.activate(root.model.getDataAt(currentIndex))
        delegate: Item {
            id: tile
            required property var model
            required property int index
            width: grid.cellWidth - VLCStyle.dp(20, VLCStyle.scale)
            height: grid.cellHeight - VLCStyle.dp(20, VLCStyle.scale)
            Keys.onMenuPressed: {
                menu.item = tile.model; menu.row = tile.index; menu.popup()
            }
            Controls.ItemDelegate {
                id: playlistCell
                anchors.fill: parent
                visible: root.playlists
                Accessible.name: root.itemTitle(tile.model)
                highlighted: grid.currentIndex === tile.index
                background: Rectangle { radius: VLCStyle.control_radius; color: playlistCell.hovered || playlistCell.highlighted ? theme.bg.secondary : "transparent" }
                contentItem: RowLayout {
                    spacing: VLCStyle.dp(12, VLCStyle.scale)
                    Widgets.IconLabel { text: VLCIcons.media_playlist; color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
                    Widgets.MenuLabel { Layout.fillWidth: true; text: root.itemTitle(tile.model); color: theme.fg.primary; font.pixelSize: VLCStyle.dp(14, VLCStyle.scale); font.weight: Font.Normal; elide: Text.ElideRight }
                    Widgets.IconToolButton { text: VLCIcons.more; description: qsTr("Playlist actions"); onClicked: { menu.item = tile.model; menu.row = tile.index; menu.popup() } }
                }
                onClicked: { grid.currentIndex = tile.index; History.setViewProperty("initialIndex", tile.index); root.activate(tile.model) }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: { grid.currentIndex = tile.index; menu.item = tile.model; menu.row = tile.index; menu.popup() }
                }
            }
            Widgets.GridItem {
            property var model: tile.model
            property int index: tile.index
            visible: !root.playlists
            view: grid
            width: grid.cellWidth - VLCStyle.dp(20, VLCStyle.scale)
            height: grid.cellHeight - VLCStyle.dp(20, VLCStyle.scale)
            pictureWidth: width
            pictureHeight: VLCStyle.dp(root.playlists ? 178 : 200, VLCStyle.scale)
            pictureRadius: VLCStyle.dp(12, VLCStyle.scale)
            title: root.itemTitle(model)
            titleFont.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
            titleFont.weight: Font.Normal
            subtitle: model.main_artist || ""
            subtitleFont.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
            subtitleFont.weight: Font.Normal
            image: model.thumbnail || model.cover || model.small_cover || ""
            fallbackImage: VLCStyle.noArtAlbumCover
            activateOnSingleClick: true
            selected: grid.currentIndex === index
            onItemClicked: { grid.currentIndex = index; History.setViewProperty("initialIndex", index) }
            onItemDoubleClicked: root.activate(model)
            onPlayClicked: root.activate(model)
            onContextMenuButtonClicked: { menu.item = model; menu.row = index; menu.popup() }
            }
        }
        Controls.ScrollBar.vertical: Widgets.ScrollBarExt {}
    }
    ListView {
        id: list
        anchors { fill: parent; margins: VLCStyle.dp(20, VLCStyle.scale) }
        clip: true
        visible: !MainCtx.gridView
        focus: visible
        model: root.model
        spacing: VLCStyle.dp(10, VLCStyle.scale)
        currentIndex: root.initialIndex
        Keys.onReturnPressed: root.activate(root.model.getDataAt(currentIndex))
        Keys.onSpacePressed: root.activate(root.model.getDataAt(currentIndex))
        delegate: Controls.ItemDelegate {
            id: listCell
            required property var model
            required property int index
            width: list.width
            height: VLCStyle.dp(32, VLCStyle.scale)
            highlighted: list.currentIndex === index
            readonly property ColorContext colorContext: ColorContext { id: cellTheme; palette: VLCStyle.palette; colorSet: ColorContext.Item; hovered: listCell.hovered; focused: listCell.visualFocus }
            background: Rectangle { color: parent.highlighted ? cellTheme.bg.highlight : parent.hovered ? cellTheme.bg.secondary : "transparent"; radius: VLCStyle.control_radius }
            contentItem: RowLayout {
                spacing: VLCStyle.dp(10, VLCStyle.scale)
                Widgets.IconLabel { visible: root.playlists || root.collections; text: root.playlists ? VLCIcons.media_playlist : VLCIcons.topbar_music; color: theme.fg.secondary }
                Image { visible: !root.playlists && !root.collections; Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale); Layout.preferredHeight: Layout.preferredWidth; source: model.small_cover || model.cover || VLCStyle.noArtAlbumCover; fillMode: Image.PreserveAspectCrop }
                Widgets.MenuLabel { Layout.fillWidth: true; text: root.itemTitle(model) + (model.main_artist ? " - " + model.main_artist : ""); color: theme.fg.primary; font.pixelSize: VLCStyle.dp(16, VLCStyle.scale); elide: Text.ElideRight }
                Widgets.IconToolButton { text: VLCIcons.more; description: qsTr("Item actions"); onClicked: { menu.item = model; menu.row = index; menu.popup() } }
            }
            onClicked: { list.currentIndex = index; History.setViewProperty("initialIndex", index); root.activate(model) }
        }
        Controls.ScrollBar.vertical: Widgets.ScrollBarExt {}
    }
    Widgets.MenuLabel {
        anchors.centerIn: parent
        visible: !root.model || root.model.count === 0
        text: root.model?.loading ? qsTr("Loading…") : root.searchText !== "" ? qsTr("No matching items") : root.emptyText
        color: theme.fg.secondary
        wrapMode: Text.WordWrap
    }
    Component {
        id: createTile
        Widgets.ButtonExt {
            width: grid.width
            height: VLCStyle.dp(48, VLCStyle.scale)
            text: qsTr("+ Create playlist")
            onClicked: root.addRequested()
        }
    }
    Widgets.LibraryPageHeader.LibraryMenu {
        id: menu
        property var item: null
        property int row: -1
        width: VLCStyle.dp(265, VLCStyle.scale)
        Widgets.LibraryPageHeader.LibraryMenuItem { accentIcon: true; text: qsTr("Play"); iconText: VLCIcons.play; onTriggered: MediaLib.addAndPlay(menu.item.id) }
        Widgets.LibraryPageHeader.LibraryMenuItem { accentIcon: true; text: qsTr("Add to Queue"); iconText: VLCIcons.queue; onTriggered: MediaLib.addToPlaylist(menu.item.id) }
        Widgets.LibraryPageHeader.LibraryMenuItem { visible: root.playlists; height: visible ? implicitHeight : 0; text: qsTr("Rename"); iconText: VLCIcons.edit; onTriggered: root.model.showDialogRename(root.model.index(menu.row, 0)) }
        Widgets.LibraryPageHeader.LibraryMenuItem { visible: root.playlists; height: visible ? implicitHeight : 0; text: qsTr("Delete playlist"); iconText: VLCIcons.remove; onTriggered: { if (DialogsProvider.questionDialog(qsTr("Delete this playlist?"), qsTr("Delete playlist"))) root.model.deletePlaylists([menu.item.id]) } }
    }
}
