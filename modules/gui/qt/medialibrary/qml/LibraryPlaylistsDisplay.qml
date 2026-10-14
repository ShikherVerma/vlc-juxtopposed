import QtQuick
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Widgets as Widgets
import VLC.Style

Widgets.PageLoader {
    id: root
    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0
    property bool enableBeginningFade: true
    property bool enableEndFade: true
    pageModel: [{name: "all", default: true, component: all}, {name: "list", component: items}]
    Component {
        id: all
        LibraryCollectionPage {
            id: page
            title: qsTr("Playlists")
            playlists: true
            breadcrumbs: [{title: qsTr("Playlists"), path: ["playlists", "all"]}]
            emptyText: qsTr("No saved playlists. Create a playlist to get started.")
            addDescription: qsTr("Create playlist")
            sortModel: [{text: qsTr("Title"), criteria: "title"}]
            model: MLPlaylistListModel { ml: MediaLib; playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL; searchPattern: page.search.pattern; sortCriteria: page.sort.criteria; sortOrder: page.sort.order; coverSize: Qt.size(632, 356); coverDefault: VLCStyle.noArtAlbumCover }
            onAddRequested: dialog.open()
            onActivated: item => History.push(["playlists", "list"], {parentId: item.id, title: item.name})
            LibraryInputDialog { id: dialog }
        }
    }
    Component {
        id: items
        PlaylistMediaDisplay {
            id: page
            property string searchText: ""
            isMusic: false
            searchPattern: page.search.pattern
            sortOrder: page.sort.order
            sortCriteria: page.sort.criteria
            header: Widgets.LibraryPageHeader {
                breadcrumbs: [{title: qsTr("Playlists"), path: ["playlists", "all"]}, {title: page.title, path: ["playlists", "list"], properties: {parentId: page.parentId, title: page.title}}]
                count: page.model.count
                search: page.search
                sort: page.sort
                searchText: page.searchText
                gridAvailable: false
                onQueryEdited: query => { page.searchText = query; page.search.pattern = query; History.setViewProperty("searchText", query) }
            }
            Component.onCompleted: Qt.callLater(() => { page.search.pattern = page.searchText })
        }
    }
}
