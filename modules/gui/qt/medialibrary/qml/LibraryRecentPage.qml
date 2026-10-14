import QtQuick
import QtQuick.Layouts
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Style
import VLC.Widgets as Widgets

LibraryCollectionPage {
    id: root
    title: qsTr("Recent")
    breadcrumbs: [{title: qsTr("Recent"), path: ["recent"]}]
    emptyText: qsTr("No recently played media.")
    sortModel: [] // Keep the history API's real recency order.
    model: MLRecentMediaModel { id: history; ml: MediaLib; searchPattern: root.search.pattern; sortCriteria: "id"; sortOrder: Qt.DescendingOrder }
    footer: RowLayout {
        height: VLCStyle.dp(50, VLCStyle.scale)
        Item { Layout.fillWidth: true }
        Widgets.ButtonExt {
            text: qsTr("Clear history")
            enabled: history.count > 0
            onClicked: { if (DialogsProvider.questionDialog(qsTr("Clear playback history?"), qsTr("Clear history"))) history.clearHistory() }
        }
        Item { width: VLCStyle.dp(20, VLCStyle.scale) }
    }
}
