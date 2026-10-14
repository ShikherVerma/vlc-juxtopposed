import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls.Basic as Controls
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Playlist
import VLC.Dialogs
import VLC.Style
import VLC.Widgets as Widgets

Window {
    id: root
    property bool network: false
    property var files: []
    property url artwork: ""
    property string errorText: ""
    readonly property var urls: links.text.split(/\r?\n/).map(x => x.trim()).filter(x => x.length > 0)
    readonly property bool urlsValid: urls.length > 0 && urls.every(x => /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\/\S+$/.test(x) && !/^file:/i.test(x))
    title: network ? qsTr("Open Network Stream") : qsTr("New Playlist")
    transientParent: MainCtx.intfMainWindow
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    width: VLCStyle.dp(network ? 1048 : 713, VLCStyle.scale)
    height: VLCStyle.dp(network ? 631 : 589, VLCStyle.scale)
    // Native X11 without a compositor otherwise backs transparent corners
    // with black even in Light. Use the named opaque Light dialog surface.
    color: VLCStyle.palette.isDark ? "transparent" : lightSurface.bg.primary
    readonly property ColorContext colorContext: body.colorContext
    function open() { if (playlists.transactionPending) return; titleField.text = ""; links.text = ""; files = []; artwork = ""; errorText = ""; show() }
    function addFiles(urls) { files = [...files, ...Array.from(urls, x => x.toString())] }
    function save() {
        errorText = ""
        if (network && titleField.text.trim() === "") {
            MainPlaylistController.append(urls, true)
            close()
        } else {
            playlists.create(titleField.text.trim(), network ? urls : files, artwork)
        }
    }
    MLPlaylistListModel {
        id: playlists
        ml: MainCtx.mediaLibraryAvailable ? MediaLib : null
        onPlaylistCreated: {
            if (root.network) MainPlaylistController.append(root.urls, true)
            root.close()
        }
        onPlaylistCreationFailed: root.errorText = qsTr("The playlist could not be created. Please try again.")
    }
    function dp(value) { return VLCStyle.dp(value, VLCStyle.scale) }
    Shortcut { sequence: "Escape"; onActivated: root.close() }
    Item {
        id: body
        anchors.fill: parent
        readonly property ColorContext colorContext: ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.View }
        ColorContext { id: dialogSurface; palette: VLCStyle.palette; colorSet: ColorContext.ComboBox }
        ColorContext { id: titleTheme; palette: VLCStyle.palette; colorSet: ColorContext.TabButton }
        ColorContext { id: placeholderTheme; palette: VLCStyle.palette; colorSet: ColorContext.TextField; enabled: false }
        ColorContext { id: lightSurface; palette: VLCStyle.palette; colorSet: ColorContext.Window }
        Rectangle { anchors.fill: parent; color: VLCStyle.palette.isDark ? dialogSurface.bg.primary : lightSurface.bg.primary; radius: VLCStyle.dp(14, VLCStyle.scale) }
        Rectangle {
            x: 0; y: 0; width: parent.width; height: root.dp(50)
            color: theme.bg.secondary
            radius: root.dp(14)
            Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: root.dp(20); color: theme.bg.secondary }
            DragHandler {
                target: null
                property point start
                onActiveChanged: { if (active) start = Qt.point(root.x, root.y) }
                onTranslationChanged: { root.x = start.x + translation.x; root.y = start.y + translation.y }
            }
            Widgets.MenuLabel { x: root.dp(20); anchors.verticalCenter: parent.verticalCenter; text: root.title; color: titleTheme.fg.primary; font.pixelSize: root.dp(14); font.weight: Font.Normal }
            Widgets.IconToolButton { anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: root.dp(12) } text: VLCIcons.close; description: qsTr("Close"); enabled: !playlists.transactionPending; onClicked: root.close() }
        }
        // Figma dialog buttons are faint lifts of the dialog surface, not
        // the darker standard button surface. Keep palette hover/focus states.
        component DialogButton: Widgets.ButtonExt {
            id: button
            color: !enabled ? (theme.palette.isDark ? Qt.rgba(titleTheme.fg.primary.r, titleTheme.fg.primary.g, titleTheme.fg.primary.b, 0.45) : colorContext.fg.primary)
                            : hovered || visualFocus ? colorContext.fg.primary : titleTheme.fg.primary
            background: Rectangle {
                radius: button.radius
                color: button.hovered ? button.colorContext.bg.secondary
                     : theme.palette.isDark ? Qt.lighter(dialogSurface.bg.primary, button.enabled ? 1.08 : 1.2)
                     : button.colorContext.bg.primary
                border.width: button.visualFocus ? 1 : 0
                border.color: button.colorFocus
            }
        }
        component Label: Widgets.MenuLabel {
            color: titleTheme.fg.secondary
            font.pixelSize: root.dp(16)
            font.weight: Font.Normal
        }
        component Description: Widgets.MenuLabel {
            color: theme.fg.secondary
            font.pixelSize: root.dp(14)
            font.weight: Font.Normal
        }
        Label { x: root.dp(20); y: root.dp(80); text: root.network ? qsTr("Network URL") : qsTr("Playlist Details") }
        Description {
            x: root.dp(20); y: root.dp(112)
            width: parent.width - root.dp(root.network ? 40 : 170)
            elide: Text.ElideRight
            text: root.network ? qsTr("You can enter multiple links to load, one per line.") : qsTr("The playlist will be saved and displayed with this title and thumbnail.")
        }
        Controls.ScrollView {
            visible: root.network
            x: root.dp(22); y: root.dp(152); width: parent.width - root.dp(44); height: root.dp(190)
            Controls.TextArea {
                id: links
                placeholderText: "e.g., https://www.example.com/stream.avi"
                color: theme.fg.primary
                placeholderTextColor: VLCStyle.palette.isDark ? placeholderTheme.fg.primary : theme.fg.secondary
                font.pixelSize: root.dp(14)
                padding: root.dp(12)
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: theme.bg.secondary; radius: VLCStyle.control_radius }
                Accessible.name: qsTr("Network URLs, one per line")
            }
        }
        Controls.Button {
            visible: !root.network
            x: parent.width - root.dp(130); y: root.dp(81); width: root.dp(108); height: width
            text: "+"
            Accessible.name: qsTr("Select playlist thumbnail")
            contentItem: Item {
                Rectangle { anchors.centerIn: parent; width: root.dp(30); height: width; radius: root.dp(4); color: theme.palette.isDark ? Qt.darker(dialogSurface.bg.primary, 1.08) : theme.bg.primary; visible: root.artwork.toString() === ""
                    Widgets.IconLabel { anchors.centerIn: parent; text: VLCIcons.add; color: theme.fg.secondary; font.pixelSize: root.dp(24) }
                }
            }
            background: Rectangle { color: theme.bg.secondary; radius: VLCStyle.control_radius; Image { anchors.fill: parent; source: root.artwork; fillMode: Image.PreserveAspectCrop } }
            onClicked: { const picked = DialogsProvider.pickPlaylistFiles(true); if (picked.length) root.artwork = picked[0] }
        }
        Rectangle { x: root.dp(20); y: root.dp(root.network ? 363 : 211); width: parent.width - root.dp(40); height: 1; color: theme.separator }
        Label { visible: root.network; x: root.dp(20); y: root.dp(392); text: qsTr("Playlist Name (optional)") }
        Description { visible: root.network; x: root.dp(20); y: root.dp(424); text: qsTr("The playlist will be saved and displayed with this title.") }
        Widgets.TextFieldExt {
            id: titleField
            x: root.dp(22); y: root.dp(root.network ? 466 : 154)
            width: parent.width - root.dp(root.network ? 44 : 176); height: root.dp(34)
            placeholderText: root.network ? qsTr("e.g., Favorite TV Channels") : qsTr("e.g., Favorite Videos")
            placeholderTextColor: VLCStyle.palette.isDark ? placeholderTheme.fg.primary : theme.fg.secondary
            Accessible.name: qsTr("Playlist title")
            enabled: MainCtx.mediaLibraryAvailable
            bgColor: theme.bg.secondary
        }
        Label { visible: !root.network; x: root.dp(20); y: root.dp(239); text: qsTr("Select Files") }
        Description { visible: !root.network; x: root.dp(20); y: root.dp(271); text: qsTr("These files will be added to the playlist.") }
        Controls.Button {
            visible: !root.network
            x: root.dp(20); y: root.dp(310); width: parent.width - root.dp(40); height: parent.height - root.dp(404)
            text: root.files.length ? qsTr("%1 files selected — click to add more").arg(root.files.length) : qsTr("Drag files into the box or click here to select files.")
            Accessible.name: qsTr("Select playlist files")
            contentItem: Item {
                Widgets.MenuLabel { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: root.dp(50) } text: parent.parent.text; color: theme.fg.secondary; font.pixelSize: root.dp(14); font.weight: Font.Normal }
                Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: root.dp(95) } width: root.dp(84); height: root.dp(30); radius: root.dp(4); color: theme.palette.isDark ? Qt.darker(dialogSurface.bg.primary, 1.08) : theme.bg.primary
                    Widgets.IconLabel { anchors.centerIn: parent; text: VLCIcons.add; color: theme.fg.secondary; font.pixelSize: root.dp(24) }
                }
            }
            background: Rectangle {
                id: dropBorder
                color: theme.bg.secondary
                radius: VLCStyle.control_radius
                // Tiled rectangles stay compatible with static Qt (no vector scene graph).
                Repeater {
                    model: Math.max(0, Math.floor((dropBorder.width - root.dp(4)) / root.dp(6)))
                    delegate: Rectangle {
                        required property int index
                        x: root.dp(4 + index * 6); y: 0
                        width: root.dp(3); height: root.dp(2); color: theme.separator
                        Rectangle { y: dropBorder.height - height; width: parent.width; height: root.dp(2); color: theme.separator }
                    }
                }
                Repeater {
                    model: Math.max(0, Math.floor((dropBorder.height - root.dp(4)) / root.dp(6)))
                    delegate: Rectangle {
                        required property int index
                        x: 0; y: root.dp(4 + index * 6)
                        width: root.dp(2); height: root.dp(3); color: theme.separator
                        Rectangle { x: dropBorder.width - width; width: root.dp(2); height: parent.height; color: theme.separator }
                    }
                }
            }
            onClicked: root.addFiles(DialogsProvider.pickPlaylistFiles(false))
            DropArea { anchors.fill: parent; onEntered: drag => { drag.accepted = drag.hasUrls }; onDropped: drop => { if (drop.hasUrls) { root.addFiles(drop.urls); drop.acceptProposedAction() } } }
        }
        Description { x: root.dp(20); y: parent.height - root.dp(88); width: parent.width - root.dp(40); text: root.errorText; visible: text !== "" }
        DialogButton { x: root.dp(20); y: parent.height - root.dp(52); width: root.dp(100); height: root.dp(32); text: qsTr("Cancel"); enabled: !playlists.transactionPending; onClicked: root.close() }
        DialogButton {
            x: parent.width - root.dp(120); y: parent.height - root.dp(52); width: root.dp(100); height: root.dp(32)
            text: qsTr("Save")
            enabled: !playlists.transactionPending && (root.network ? root.urlsValid : titleField.text.trim() !== "") && (titleField.text.trim() === "" || MainCtx.mediaLibraryAvailable)
            onClicked: root.save()
        }
    }
}
