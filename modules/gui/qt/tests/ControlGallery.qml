// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VLC.Style
import VLC.Widgets as W

Window {
    id: win
    width: 2440
    height: 1650
    visible: true
    title: "Lane 1.4 production controls"
    color: VLCStyle.palette.isDark ? "#282828" : "#e7e7e7"
    property var states: ["normal", "hover", "pressed", "focused", "disabled", "checked"]
    property var names: ["ButtonExt", "ActionButtonPrimary", "IconToolButton", "IconButton", "CheckButton", "CheckBoxExt", "ComboBoxExt", "SliderExt horizontal", "SliderExt vertical", "SpinBoxExt", "TextFieldExt", "CheckedDelegate", "BannerTabButton", "ScrollBarExt", "ToolTipExt", "SearchBox", "RadioButtonExt", "RoundButtonExt", "TextToolButton", "PopupBackground", "Partial checkbox"]
    property int focusRow: -1

    Row {
        x: 200; y: 20
        Repeater {
            model: win.states
            Text { required property string modelData; width: 370; text: modelData; color: galleryDark ? "#c6c6c6" : "#161616"; font.family: "Inter"; font.pixelSize: 16 }
        }
    }
    Column {
        readonly property ColorContext colorContext: ColorContext { palette: VLCStyle.palette; colorSet: ColorContext.View }
        x: 8; y: 64
        Repeater {
            model: win.names
            Item {
                id: row
                required property int index
                required property string modelData
                width: win.width; height: 68
                Text { width: 185; text: row.modelData; color: galleryDark ? "#969696" : "#3c3c3c"; font.family: "Inter"; font.pixelSize: 12 }
                Row {
                    x: 192
                    Repeater {
                        model: win.states
                        Item {
                            id: cell
                            required property int index
                            required property string modelData
                            width: 370; height: 64
                            Loader {
                                id: loader
                                anchors.verticalCenter: parent.verticalCenter
                                sourceComponent: [button, primary, tool, icon, toggle, checkbox, combo, slider, vertical, spin, field, option, tab, scrollbar, tip, search, radio, round, textTool, popupBg, partial][row.index]
                                onLoaded: {
                                    item.enabled = cell.modelData !== "disabled"
                                    if (item.colorContext) {
                                        item.colorContext.hovered = Qt.binding(() => cell.modelData === "hover")
                                        item.colorContext.pressed = Qt.binding(() => cell.modelData === "pressed")
                                    }
                                    if (item.hasOwnProperty("checked") && row.index !== 20) item.checked = cell.modelData === "checked"
                                    if (item.hasOwnProperty("selected")) item.selected = cell.modelData === "checked"
                                }
                                Connections {
                                    target: win
                                    function onFocusRowChanged() {
                                        // A press opens this cell's real search popup; close it
                                        // before capturing another cell so it cannot overlap rows.
                                        if (row.index === 15 && cell.modelData === "pressed") loader.item.retract()
                                        if (win.focusRow === row.index && cell.modelData === "focused") {
                                            if (row.index === 15) {
                                                loader.item.retract()
                                                loader.item.toggleButton.forceActiveFocus(Qt.TabFocusReason)
                                            } else loader.item.forceActiveFocus(Qt.TabFocusReason)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Component { id: button; W.ButtonExt { width: 360; text: "Show Advanced Settings"; iconTxt: VLCIcons.settings } }
    Component { id: primary; W.ActionButtonPrimary { width: 360; text: "Action"; iconTxt: VLCIcons.add } }
    Component { id: tool; W.IconToolButton { text: VLCIcons.settings; description: "Settings" } }
    Component { id: icon; W.IconButton { text: VLCIcons.settings; description: "Settings" } }
    Component { id: toggle; W.CheckButton {} }
    Component { id: checkbox; W.CheckBoxExt { text: "Option" } }
    Component { id: combo; W.ComboBoxExt { width: 362; model: ["Dropdown", "Second", "Third"]; currentIndex: 0 } }
    Component { id: slider; W.SliderExt { width: 320; value: 0.5 } }
    Component { id: vertical; W.SliderExt { orientation: Qt.Vertical; height: 52; value: 0.5 } }
    Component { id: spin; W.SpinBoxExt { width: 180; editable: true; value: 20 } }
    Component { id: field; W.TextFieldExt { width: 200; text: "Text field" } }
    Component { id: option; W.CheckedDelegate { width: 261; text: "Option" } }
    Component { id: tab; W.BannerTabButton { width: 213; text: "Relative"; showCurrentIndicator: false } }
    Component { id: scrollbar; W.ScrollBarExt { orientation: Qt.Horizontal; width: 320; size: 0.3; policy: ScrollBar.AlwaysOn } }
    // Expose the actual tooltip content/background as a normal item in the gallery,
    // without opening sixteen independent Popup.Window tooltips above one another.
    Component { id: tip; Item { width: 180; height: 32; W.ToolTipExt { id: tooltip; text: "Tooltip"; colorContext.palette: VLCStyle.palette } Component.onCompleted: { tooltip.background.parent = this; tooltip.background.width = 100; tooltip.background.height = 31; tooltip.contentItem.parent = this; tooltip.contentItem.x = 8; tooltip.contentItem.y = 8 } } }
    Component { id: search; W.SearchBox { width: 32; height: 32 } }
    Component { id: radio; W.RadioButtonExt { text: "Option"; autoExclusive: false } }
    Component { id: round; W.RoundButtonExt { text: "A" } }
    Component { id: textTool; W.TextToolButton { text: "Option" } }
    Component { id: popupBg; W.PopupBackground { width: 320; height: 48; backdrop: null } }
    Component { id: partial; W.CheckBoxExt { text: "Partial"; tristate: true; checkState: Qt.PartiallyChecked } }
}
