import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Style
import VLC.Widgets as Widgets

LibraryCollectionPage {
    id: root
    property string kind: "tracks"
    property alias parentId: audio.parentId
    property var ancestors: []
    // MLAlbumModel accepts only Artist/Genre parents. An album's own ID is
    // a tracks parent, never an albums parent (even during History restore).
    readonly property var albumParent: /^(VLC_ML_PARENT_ARTIST|VLC_ML_PARENT_GENRE) -/.test(audio.parentId.toString())
        ? audio.parentId : MediaLib.deserializeMlItemIdFromString("")
    readonly property var tabs: [
        { title: qsTr("Songs"), name: "tracks" }, { title: qsTr("Albums"), name: "albums" },
        { title: qsTr("Artists"), name: "artists" }, { title: qsTr("Genres"), name: "genres" }
    ]
    title: qsTr("Music")
    collections: kind !== "tracks"
    model: kind === "tracks" ? audio : kind === "albums" ? albums : kind === "artists" ? artists : genres
    breadcrumbs: [{title: qsTr("Music"), path: ["music", "tracks"]}, ...ancestors.map((crumb, index) => Object.assign({}, crumb, {properties: Object.assign({}, crumb.properties, {ancestors: ancestors.slice(0, index + 1)})}))]
    sortModel: kind === "artists" ? [{text: qsTr("Name"), criteria: "name"}] : kind === "genres" ? [{text: qsTr("Name"), criteria: "name"}] : [
        {text: qsTr("Title"), criteria: "title"}, {text: qsTr("Artist"), criteria: "main_artist"}, {text: qsTr("Duration"), criteria: "duration"}
    ]
    onActivated: item => {
        const nextKind = kind === "albums" ? "tracks" : "albums"
        const path = ["music", "collection"]
        const properties = {kind: nextKind, parentId: item.id}
        History.push(path, Object.assign({}, properties, {ancestors: [...ancestors, {title: itemTitle(item), path: path, properties: properties}]}), Qt.MouseFocusReason)
    }
    header: Column {
        Widgets.LibraryPageHeader {
            width: parent.width
            breadcrumbs: root.breadcrumbs
            count: root.model.count
            search: root.search
            sort: root.sort
            searchText: root.searchText
            onQueryEdited: query => { root.searchText = query; History.setViewProperty("searchText", query) }
        }
        RowLayout {
            x: VLCStyle.dp(20, VLCStyle.scale)
            height: VLCStyle.dp(48, VLCStyle.scale)
            spacing: VLCStyle.dp(20, VLCStyle.scale)
            Repeater {
                model: root.tabs
                Controls.ToolButton {
                    required property var modelData
                    text: modelData.title
                    height: VLCStyle.dp(48, VLCStyle.scale)
                    background: Rectangle {
                        color: parent.hovered ? root.colorContext.bg.secondary : "transparent"
                        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: VLCStyle.dp(2, VLCStyle.scale); color: root.colorContext.accent; visible: root.kind === modelData.name }
                    }
                    contentItem: Widgets.MenuLabel { text: parent.text; color: root.kind === modelData.name ? root.colorContext.accent : root.colorContext.fg.secondary; font.pixelSize: VLCStyle.dp(16, VLCStyle.scale); font.weight: Font.Normal }
                    onClicked: History.push(["music", modelData.name])
                }
            }
        }
    }
    MLAudioModel { id: audio; ml: MediaLib; searchPattern: root.search.pattern; sortCriteria: root.sort.criteria; sortOrder: root.sort.order }
    MLAlbumModel { id: albums; ml: MediaLib; parentId: root.albumParent; searchPattern: root.search.pattern; sortCriteria: root.sort.criteria; sortOrder: root.sort.order }
    MLArtistModel { id: artists; ml: MediaLib; searchPattern: root.search.pattern; sortCriteria: "name"; sortOrder: root.sort.order }
    MLGenreModel { id: genres; ml: MediaLib; searchPattern: root.search.pattern; sortCriteria: "name"; sortOrder: root.sort.order }
}
