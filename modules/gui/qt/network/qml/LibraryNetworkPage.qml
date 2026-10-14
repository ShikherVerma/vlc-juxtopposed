import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Network
import VLC.Style
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root
    title: qsTr("Network")
    hasGridListMode: false
    isSearchable: true
    property string searchText: ""
    readonly property int serviceCount: Math.max(0, sources.count - (searchText === "" ? 1 : 0))
    readonly property ColorContext colorContext: ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.View }
    header: Widgets.LibraryPageHeader {
        breadcrumbs: [{title: qsTr("Network"), path: ["network", "home"]}]
        count: root.serviceCount
        search: root.search
        searchText: root.searchText
        gridAvailable: false
        onQueryEdited: query => { root.searchText = query; root.search.pattern = query; History.setViewProperty("searchText", query) }
    }
    Component.onCompleted: Qt.callLater(() => { root.search.pattern = root.searchText })
    ColumnLayout {
        anchors { fill: parent; margins: VLCStyle.dp(20, VLCStyle.scale) }
        spacing: VLCStyle.dp(20, VLCStyle.scale)
        RowLayout {
            spacing: VLCStyle.dp(20, VLCStyle.scale)
            Widgets.GridItem {
                view: root
                Layout.preferredWidth: VLCStyle.dp(200, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(240, VLCStyle.scale)
                pictureWidth: VLCStyle.dp(200, VLCStyle.scale)
                pictureHeight: pictureWidth
                pictureRadius: VLCStyle.dp(12, VLCStyle.scale)
                title: qsTr("Add Network Stream")
                titleFont.weight: Font.Normal
                titleFont.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                subtitle: ""
                playCoverShowPlay: false
                activateOnSingleClick: true
                image: ""
                pictureOverlay: Widgets.IconLabel { text: VLCIcons.add; font.pixelSize: VLCStyle.dp(40, VLCStyle.scale); color: theme.fg.secondary; anchors.centerIn: parent }
                onItemDoubleClicked: dialog.open()
                onPlayClicked: dialog.open()
            }
            Widgets.ButtonExt { text: qsTr("Browse LAN / UPnP / SMB"); onClicked: History.push(["network", "lan"]) }
            Widgets.ButtonExt { text: qsTr("Manage services"); onClicked: History.push(["discover", "services", "services_manage"]) }
        }
        Widgets.MenuLabel { text: qsTr("Internet services"); color: theme.fg.primary; font.pixelSize: VLCStyle.dp(20, VLCStyle.scale); font.weight: Font.Normal }
        Widgets.MenuLabel {
            visible: root.serviceCount === 0
            text: root.searchText !== "" ? qsTr("No matching services") : qsTr("No internet services are available in this VLC build. Podcasts and Radio appear only when provided by services discovery.")
            color: theme.fg.secondary
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: NetworkSourcesModel { id: sources; ctx: MainCtx; searchPattern: root.search.pattern }
            delegate: Controls.ItemDelegate {
                required property var model
                width: ListView.view.width
                visible: model.type !== NetworkSourcesModel.TYPE_DUMMY
                height: visible ? VLCStyle.dp(42, VLCStyle.scale) : 0
                contentItem: Widgets.MenuLabel { text: model.long_name; color: theme.fg.primary; font.pixelSize: VLCStyle.dp(16, VLCStyle.scale); font.weight: Font.Normal; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? theme.bg.highlight : "transparent"; radius: VLCStyle.control_radius }
                onClicked: History.push(["discover", "services", "source_root"], {source_name: model.name})
            }
            Controls.ScrollBar.vertical: Widgets.ScrollBarExt {}
        }
    }
    LibraryInputDialog { id: dialog; network: true }
}
