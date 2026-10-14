import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

Item {
    id: root
    property var breadcrumbs: [] // {title, path, properties}
    property int count: 0
    property var sort: null
    property var search: null
    property bool gridAvailable: true
    property string searchText: ""
    property string addDescription: ""
    signal addRequested()
    signal queryEdited(string query)
    implicitHeight: VLCStyle.dp(72, VLCStyle.scale)
    readonly property ColorContext colorContext: ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.View }
    // Same popup vocabulary as the opt-in Lane 3.2 folder header.
    component LibraryMenu: Controls.Menu {
        width: VLCStyle.dp(265, VLCStyle.scale)
        padding: VLCStyle.dp(4, VLCStyle.scale)
        background: Widgets.PopupBackground { radius: VLCStyle.contextMenu_radius; colorContext.palette: VLCStyle.palette }
    }
    component LibraryMenuItem: Controls.MenuItem {
        id: entry
        property string iconText: ""
        property bool accentIcon: false
        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)
        leftPadding: VLCStyle.dp(14, VLCStyle.scale)
        rightPadding: VLCStyle.dp(14, VLCStyle.scale)
        indicator: null
        arrow: null
        readonly property ColorContext colorContext: ColorContext { id: menuTheme; palette: VLCStyle.palette; colorSet: ColorContext.ComboBox; hovered: entry.highlighted; enabled: entry.enabled }
        background: Rectangle { radius: VLCStyle.control_radius; color: entry.highlighted || entry.checked ? menuTheme.bg.secondary : "transparent" }
        contentItem: RowLayout {
            spacing: VLCStyle.dp(10, VLCStyle.scale)
            Widgets.IconLabel { visible: entry.iconText !== ""; text: entry.iconText; color: entry.accentIcon ? VLCStyle.accentForeground(VLCStyle.palette) : menuTheme.fg.primary; font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
            Widgets.MenuLabel { Layout.fillWidth: true; text: entry.text; color: menuTheme.fg.primary; font.pixelSize: VLCStyle.dp(14, VLCStyle.scale); font.weight: Font.Normal }
            Widgets.IconLabel { visible: entry.checked; text: VLCIcons.check; color: VLCStyle.accentForeground(VLCStyle.palette); font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
        }
    }
    Connections { target: root.search; function onAskShow() { field.forceActiveFocus(Qt.ShortcutFocusReason) } }
    Connections { target: root.sort; function onAskShow() { sortMenu.open() } }
    RowLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: VLCStyle.dp(20, VLCStyle.scale) }
        height: VLCStyle.dp(34, VLCStyle.scale)
        spacing: VLCStyle.dp(8, VLCStyle.scale)
        Widgets.IconToolButton { Layout.preferredWidth: root.width < VLCStyle.dp(700) ? VLCStyle.dp(28) : implicitWidth; text: VLCIcons.back; description: qsTr("Back"); enabled: !History.previousEmpty; onClicked: History.previous(Qt.MouseFocusReason) }
        Widgets.IconToolButton { Layout.preferredWidth: root.width < VLCStyle.dp(700) ? VLCStyle.dp(28) : implicitWidth; text: VLCIcons.back; rotation: 180; description: qsTr("Forward"); enabled: !History.nextEmpty; onClicked: History.next(Qt.MouseFocusReason) }
        Flickable {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            clip: true
            contentWidth: breadcrumbRow.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            RowLayout {
                id: breadcrumbRow
                height: parent.height
                spacing: VLCStyle.dp(8)
        Repeater {
            model: root.breadcrumbs
            RowLayout {
                required property var modelData
                required property int index
                spacing: VLCStyle.dp(4, VLCStyle.scale)
                Widgets.MenuLabel { visible: index > 0; text: "›"; color: theme.fg.secondary }
                Controls.ToolButton {
                    id: breadcrumbButton
                    text: modelData.title
                    contentItem: Widgets.MenuLabel { text: parent.text; color: theme.fg.primary; font.pixelSize: VLCStyle.dp(16, VLCStyle.scale); font.weight: Font.Normal; elide: Text.ElideMiddle }
                    readonly property ColorContext colorContext: ColorContext {
                        id: breadcrumbTheme
                        colorSet: ColorContext.ToolButton
                        hovered: breadcrumbButton.hovered
                        pressed: breadcrumbButton.down
                        focused: breadcrumbButton.visualFocus
                    }
                    background: Rectangle { color: breadcrumbTheme.bg.primary; radius: VLCStyle.control_radius; border.width: parent.visualFocus ? VLCStyle.dp(1) : 0; border.color: breadcrumbTheme.visualFocus }
                    onClicked: History.push(modelData.path, modelData.properties || {}, Qt.MouseFocusReason)
                }
            }
        }
            }
        }
        Widgets.MenuLabel { visible: root.width >= VLCStyle.dp(700); text: qsTr("%1 items").arg(root.count); color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(14, VLCStyle.scale); font.weight: Font.Normal }
        Widgets.IconToolButton {
            Layout.preferredWidth: root.width < VLCStyle.dp(700) ? VLCStyle.dp(32) : implicitWidth
            visible: root.sort?.available ?? false
            text: VLCIcons.topbar_sort; description: qsTr("Sort"); onClicked: sortMenu.open()
            LibraryMenu {
                id: sortMenu
                y: parent.height
                Repeater {
                    model: root.sort?.model ?? []
                    LibraryMenuItem { required property var modelData; text: modelData.text; checkable: true; checked: root.sort.criteria === modelData.criteria; onTriggered: root.sort.criteria = modelData.criteria }
                }
                Controls.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: theme.separator } }
                LibraryMenuItem { text: qsTr("Ascending"); checkable: true; checked: root.sort?.order === Qt.AscendingOrder; onTriggered: root.sort.order = Qt.AscendingOrder }
                LibraryMenuItem { text: qsTr("Descending"); checkable: true; checked: root.sort?.order === Qt.DescendingOrder; onTriggered: root.sort.order = Qt.DescendingOrder }
            }
        }
        Widgets.IconToolButton { visible: root.gridAvailable && root.width >= VLCStyle.dp(700); text: MainCtx.gridView ? VLCIcons.grid : VLCIcons.list; description: qsTr("Switch grid / list view"); onClicked: MainCtx.gridView = !MainCtx.gridView }
        Widgets.TextFieldExt {
            id: field
            visible: root.search?.available ?? false
            Layout.preferredWidth: Math.min(VLCStyle.dp(325, VLCStyle.scale), root.width / 4)
            Layout.fillHeight: true
            placeholderText: qsTr("Search")
            leftPadding: VLCStyle.dp(42, VLCStyle.scale)
            Widgets.IconLabel { anchors { left: parent.left; leftMargin: VLCStyle.dp(10, VLCStyle.scale); verticalCenter: parent.verticalCenter } text: VLCIcons.search; color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(24, VLCStyle.scale) }
            Accessible.name: qsTr("Search current view")
            bgColor: VLCStyle.searchInputColor(VLCStyle.palette.isDark)
            text: root.searchText
            onTextEdited: root.queryEdited(text)
        }
        Widgets.IconToolButton { Layout.preferredWidth: root.width < VLCStyle.dp(700) ? VLCStyle.dp(32) : implicitWidth; visible: root.addDescription !== ""; text: VLCIcons.add; description: root.addDescription; onClicked: root.addRequested() }
    }
    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: VLCStyle.dp(20, VLCStyle.scale); bottomMargin: 0 } height: 1; color: divider.bg.primary; ColorContext { id: divider; palette: VLCStyle.palette; colorSet: ColorContext.ComboBox } }
}
