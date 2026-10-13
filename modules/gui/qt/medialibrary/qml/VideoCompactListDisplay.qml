// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
import QtQuick
import QtQuick.Templates as T
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Util
import VLC.Style
import VLC.Widgets as Widgets

// Native 970:3308 file flow, using the existing selection/navigation view.
// No table headers: all sorting is owned by the page's SortCtx.
Widgets.ExpandGridView {
    id: flow
    property MLDragItem dragItem
    property MLContextMenu contextMenu
    signal itemDoubleClicked(var model)

    cellWidth: VLCStyle.dp(406, VLCStyle.scale)
    cellHeight: VLCStyle.dp(32, VLCStyle.scale)
    horizontalSpacing: VLCStyle.dp(10, VLCStyle.scale)
    verticalSpacing: VLCStyle.dp(10, VLCStyle.scale)
    nbItemPerRow: Math.max(1, Math.floor((_availableContentWidth + horizontalSpacing) / (cellWidth + horizontalSpacing)))
    leftMargin: VLCStyle.dp(20, VLCStyle.scale) - extraMargin
    rightMargin: VLCStyle.dp(20, VLCStyle.scale) - extraMargin
    topMargin: VLCStyle.dp(30, VLCStyle.scale)

    delegate: T.ItemDelegate {
        id: cell
        required property var model
        required property int index
        required property Item view
        property bool selected: false
        readonly property Item artworkTextureProvider: thumbnail
        width: flow.cellWidth
        height: flow.cellHeight
        padding: 0
        hoverEnabled: true
        activeFocusOnTab: true
        Accessible.role: Accessible.Cell
        Accessible.name: model.title || qsTr("Unknown title")
        Accessible.selected: selected
        Accessible.onPressAction: flow.itemDoubleClicked(model)
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.Item
            palette: VLCStyle.palette
            hovered: cell.hovered
            focused: cell.visualFocus
        }
        background: Rectangle {
            color: cell.selected ? theme.bg.highlight : "transparent"
            border.width: cell.visualFocus ? VLCStyle.dp(1, VLCStyle.scale) : 0
            border.color: theme.visualFocus
        }
        contentItem: Row {
            spacing: VLCStyle.dp(10, VLCStyle.scale)
            Item {
                width: VLCStyle.dp(32, VLCStyle.scale)
                height: width
                clip: true
                Image {
                    id: thumbnail
                    anchors.fill: parent
                    source: cell.model.thumbnail || VLCStyle.noArtVideoCover
                    fillMode: Image.PreserveAspectCrop
                }
                Widgets.VideoProgressBar {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    visible: (cell.model.progress ?? 0) > 0
                    value: Helpers.clamp(cell.model.progress ?? 0, 0, 1)
                    radius: 0
                }
                Widgets.IconLabel {
                    anchors.centerIn: parent
                    visible: !!cell.model.id && cell.model.id.hasParent()
                    text: VLCIcons.folder
                    font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
                    color: theme.fg.primary
                }
            }
            Widgets.ListLabel {
                width: flow.cellWidth - VLCStyle.dp(42, VLCStyle.scale)
                height: flow.cellHeight
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: cell.model.title || qsTr("Unknown title")
                font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                font.weight: Font.Normal
                color: theme.fg.primary
            }
        }
        function contextAt(globalPoint) {
            flow.rightClickOnItem(index)
            flow.contextMenu.popup(flow.selectionModel.selectedIndexes, globalPoint)
        }
        Keys.onMenuPressed: contextAt(mapToGlobal(0, height))
        DoubleClickIgnoringItem {
            anchors.fill: parent
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                acceptedDevices: PointerDevice.AllDevices & ~PointerDevice.TouchScreen
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onSingleTapped: (eventPoint, button) => {
                    cell.forceActiveFocus(Qt.MouseFocusReason)
                    if (button === Qt.RightButton)
                        cell.contextAt(eventPoint.globalPosition)
                    else
                        flow.leftClickOnItem(point.modifiers, cell.index, true)
                }
                onDoubleTapped: (eventPoint, button) => {
                    if (button === Qt.LeftButton)
                        flow.itemDoubleClicked(cell.model)
                }
            }
            DragHandler {
                target: null
                acceptedDevices: PointerDevice.AllDevices & ~PointerDevice.TouchScreen
                onActiveChanged: {
                    if (!flow.dragItem)
                        return
                    if (active) {
                        if (!cell.selected)
                            flow.leftClickOnItem(Qt.NoModifier, cell.index, true)
                        flow.dragItem.Drag.active = true
                    } else {
                        flow.dragItem.Drag.drop()
                    }
                }
            }
            DelegateTouchTapHandler {
                delegate: cell
                onSingleTapped: (eventPoint, button) => {
                    flow.leftClickOnItem(Qt.NoModifier, cell.index, true)
                    flow.itemDoubleClicked(cell.model)
                }
                onDoubleTapped: (eventPoint, button) => flow.itemDoubleClicked(cell.model)
                onContextMenuRequested: (index, globalPoint) => cell.contextAt(globalPoint)
            }
        }
    }
}
