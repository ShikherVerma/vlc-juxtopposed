/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets

FocusScope {
    id: root
    implicitWidth: VLCStyle.dp(400)
    property int currentTab: 0
    property bool showStatistics: false
    signal closeRequested()
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Playback and subtitle adjustments")
    AccessibleCompat.id: "adjustmentSidebar"
    readonly property ColorContext colorContext: ColorContext { id: theme; colorSet: ColorContext.View }
    readonly property ColorContext inactiveIconContext: ColorContext { id: quietIcons; colorSet: ColorContext.View; enabled: false }
    AdjustmentAdapter { id: adapter }

    function selectTab(index, enter = false, reason = Qt.TabFocusReason) {
        currentTab = index === 3 ? 3 : 0
        body.contentY = 0
        const first = currentTab === 0
            ? (statisticsToggle.enabled ? statisticsToggle : visualizationChoice.enabled ? visualizationChoice : speedSetting.enabled ? speedSetting.editor : deviceChoice)
            : (trackChoice.enabled ? trackChoice : addSubtitleButton)
        if (enter && first.enabled) first.forceActiveFocus(reason)
        else tabs.itemAt(currentTab).forceActiveFocus(reason)
    }
    function reveal(item) {
        let ancestor = item
        while (ancestor && ancestor !== body.contentItem) ancestor = ancestor.parent
        if (!ancestor) return
        const y = item.mapToItem(body.contentItem, 0, 0).y
        if (y < body.contentY) body.contentY = y
        else if (y + item.height > body.contentY + body.height) body.contentY = y + item.height - body.height
    }
    function advanceFocus(item, forward) {
        let next = item.nextItemInFocusChain(forward)
        for (let i = 0; i < 100 && next; ++i) {
            let ancestor = next
            while (ancestor && ancestor !== root) ancestor = ancestor.parent
            if (ancestor !== root) { root.selectTab(root.currentTab); return }
            if (next.enabled && next.visible && next.activeFocusOnTab) {
                next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
                root.reveal(next)
                return
            }
            next = next.nextItemInFocusChain(forward)
        }
    }
    onVisibleChanged: if (visible) {
        Player.audioDevices.refresh()
        selectTab(currentTab, true, Qt.OtherFocusReason)
    }
    Connections {
        target: Player
        function onPlayingStateChanged() { if (root.visible) Player.audioDevices.refresh() }
    }
    Navigation.parentItem: root.parent
    Navigation.cancelAction: () => root.closeRequested()
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Tab && event.modifiers === Qt.ControlModifier) {
            root.selectTab(root.currentTab === 0 ? 3 : 0)
            event.accepted = true
        } else root.Navigation.defaultKeyAction(event)
    }
    Rectangle { anchors.fill: parent; radius: VLCStyle.dp(10); color: theme.bg.primary }
    Rectangle {
        width: parent.width; height: VLCStyle.dp(50); radius: VLCStyle.dp(10); color: theme.bg.secondary
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: VLCStyle.dp(10); color: parent.color }
    }
    Row {
        id: tabRow
        x: VLCStyle.dp(6); height: VLCStyle.dp(50); spacing: VLCStyle.dp(1)
        Repeater {
            id: tabs
            model: [qsTr("Playback"), qsTr("Video"), qsTr("Audio"), qsTr("Subtitles")]
            T.Button {
                id: tab
                required property int index
                required property string modelData
                width: (root.width - VLCStyle.dp(15)) * ((index === 0 || index === 3) ? 107.5 / 385 : 85 / 385)
                height: tabRow.height
                enabled: index === 0 || index === 3
                text: modelData
                hoverEnabled: true
                activeFocusOnTab: true
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.selected: root.currentTab === index
                Accessible.description: index === 1 || index === 2 ? qsTr("Available in a later adjustment release") : ""
                onClicked: root.selectTab(index, true, Qt.MouseFocusReason)
                readonly property ColorContext colorContext: ColorContext {
                    id: tabTheme
                    colorSet: ColorContext.TabButton
                    enabled: tab.enabled
                    hovered: tab.hovered
                    pressed: tab.down
                    focused: tab.visualFocus
                }
                background: Rectangle {
                    color: tabTheme.bg.primary
                    radius: VLCStyle.dp(4)
                    border.width: tab.visualFocus ? VLCStyle.dp(1) : 0
                    border.color: theme.accent
                }
                contentItem: Item {
                  Row {
                    anchors.centerIn: parent
                    spacing: VLCStyle.dp(8)
                    Widgets.IconLabel {
                        width: VLCStyle.dp(20); height: width; font.pixelSize: width
                        text: index === 0 ? (root.currentTab === 0 ? VLCIcons.playback_two_tone_yes : VLCIcons.playback)
                            : index === 1 ? VLCIcons.video : index === 2 ? VLCIcons.audio
                            : (root.currentTab === 3 ? VLCIcons.subtitles_two_tone_yes : VLCIcons.subtitle)
                        color: root.currentTab === index ? VLCStyle.accentForeground(VLCStyle.palette) : quietIcons.fg.primary
                    }
                    Text { text: tab.text; font.pixelSize: VLCStyle.dp(12); color: tabTheme.fg.primary; y: VLCStyle.dp(2.5) }
                  }
                }
                Rectangle {
                    x: VLCStyle.dp(8); y: parent.height - VLCStyle.dp(2)
                    width: parent.width - VLCStyle.dp(16); height: VLCStyle.dp(2)
                    color: root.currentTab === index ? VLCStyle.accentForeground(VLCStyle.palette) : theme.bg.secondary
                }
                Navigation.parentItem: root
                Navigation.leftAction: () => root.selectTab(index === 0 ? 3 : 0)
                Navigation.rightAction: () => root.selectTab(index === 0 ? 3 : 0)
                Navigation.downAction: () => root.selectTab(index, true)
                Keys.onPressed: event => tab.Navigation.defaultKeyAction(event)
            }
        }
    }
    C.ScrollView {
        id: scroll
        anchors { top: tabRow.bottom; topMargin: VLCStyle.dp(1); left: parent.left; right: parent.right; bottom: parent.bottom }
        clip: true
        contentWidth: availableWidth
        C.ScrollBar.vertical: Widgets.ScrollBarExt { }
        Flickable {
            id: body
            contentWidth: width
            contentHeight: root.currentTab === 0 ? playback.height : subtitles.height
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: playback
                visible: root.currentTab === 0
                width: body.width
                topPadding: VLCStyle.dp(10)
                SettingRow {
                    label: qsTr("Show Subtitle Timeline on Seekbar")
                    height: VLCStyle.dp(52)
                    Widgets.CheckButton { enabled: false; text: parent.label; Accessible.description: qsTr("Available in a later subtitle timeline release") }
                }
                SettingRow {
                    label: qsTr("Show Statistics"); height: VLCStyle.dp(52)
                    Widgets.CheckButton {
                        id: statisticsToggle
                        enabled: adapter.statisticsAvailable && adapter.started
                        text: parent.label
                        checked: root.showStatistics
                        onToggled: root.showStatistics = checked
                        Accessible.onPressAction: if (enabled) root.showStatistics = !root.showStatistics
                        Accessible.onToggleAction: if (enabled) root.showStatistics = !root.showStatistics
                        Navigation.parentItem: root
                        Navigation.upAction: () => root.selectTab(0)
                        Navigation.downAction: () => root.advanceFocus(statisticsToggle, true)
                        activeFocusOnTab: true
                        onActiveFocusChanged: if (activeFocus) root.reveal(this)
                    }
                }
                // Reverse playback is deliberately outside the product scope.
                SettingRow {
                    label: qsTr("Sleep Timer"); height: VLCStyle.dp(78); controlCenter: VLCStyle.dp(34)
                    Widgets.ButtonExt { width: VLCStyle.dp(220); height: VLCStyle.dp(48); text: qsTr("Off"); enabled: false; Accessible.name: parent.label; Accessible.description: qsTr("Available in a later sleep timer release") }
                }
                SettingRow {
                    label: qsTr("Audio Visualizer"); height: VLCStyle.dp(68); controlCenter: VLCStyle.dp(24)
                    ModelChoice {
                        id: visualizationChoice
                        width: VLCStyle.dp(220); model: adapter.visualizations
                        enabled: adapter.visualizationsAvailable && count > 0
                        Accessible.name: parent.label
                        emptyText: qsTr("Unavailable")
                    }
                }
                NumberSetting {
                    id: speedSetting
                    label: qsTr("Speed"); height: VLCStyle.dp(126)
                    enabled: adapter.started && Player.ratechangable
                    headerHeight: 44; fieldHeight: 32; sliderOffset: 58
                    number: adapter.rate * 100; minimum: 25; maximum: 200; step: 1; sliderStep: 25; suffix: "x"; divisor: 100; decimals: 2
                    onEdited: value => adapter.setRate(value / 100)
                    // Authored quarter-step labels are also real selectable presets.
                    presets: [25, 50, 75, 100, 125, 150, 175, 200]
                }
                SettingRow {
                    label: qsTr("Output Device"); height: VLCStyle.dp(68)
                    ModelChoice { id: deviceChoice; width: VLCStyle.dp(220); model: adapter.devices; enabled: count > 0; Accessible.name: parent.label; emptyText: count === 0 ? qsTr("No output devices") : qsTr("No device reported") }
                }
                Text {
                    x: VLCStyle.dp(24); width: parent.width - VLCStyle.dp(48)
                    visible: root.showStatistics && adapter.statisticsAvailable
                    bottomPadding: VLCStyle.dp(20)
                    color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(14); wrapMode: Text.Wrap
                    text: qsTr("Statistics (current input)\nDecoded video: %1\nDisplayed pictures: %2\nLost pictures: %3\nDecoded audio: %4\nInput bitrate: %5 kb/s")
                        .arg(adapter.statistics.decodedVideo).arg(adapter.statistics.displayedPictures)
                        .arg(adapter.statistics.lostPictures).arg(adapter.statistics.decodedAudio)
                        .arg(Number(adapter.statistics.inputBitrate).toFixed(1))
                }
            }
            Column {
                id: subtitles
                visible: root.currentTab === 3
                width: body.width
                SectionLabel { text: qsTr("Embedded / external tracks (current file)") }
                SettingRow {
                    label: qsTr("Subtitle Track"); height: VLCStyle.dp(53); controlCenter: VLCStyle.dp(34)
                    ModelChoice { id: trackChoice; height: VLCStyle.dp(36); width: VLCStyle.dp(220); model: adapter.tracks; enabled: count > 0; Accessible.name: parent.label; emptyText: qsTr("Subtitles off / none selected") }
                }
                SettingRow {
                    label: qsTr("Track Language"); height: VLCStyle.dp(53); controlCenter: VLCStyle.dp(34)
                    ModelChoice { height: VLCStyle.dp(36); width: VLCStyle.dp(220); model: adapter.tracks; textRole: "language"; enabled: count > 0; Accessible.name: parent.label; emptyText: qsTr("Unspecified") }
                }
                Item { width: parent.width; height: VLCStyle.dp(15) }
                Row {
                    x: VLCStyle.dp(20); spacing: VLCStyle.dp(8); height: VLCStyle.dp(44)
                    Widgets.ButtonExt { id: addSubtitleButton; text: qsTr("Add subtitle file"); enabled: adapter.started; onClicked: adapter.addSubtitle(); Navigation.parentItem: root }
                    Widgets.ButtonExt { text: qsTr("Disable subtitles"); enabled: adapter.tracks.count > 0; onClicked: adapter.disableSubtitles(); Navigation.parentItem: root }
                }
                SectionLabel { text: qsTr("Synchronization") }
                Item { width: parent.width; height: VLCStyle.dp(10) }
                NumberSetting {
                    label: qsTr("Subtitle Track Delay"); height: VLCStyle.dp(115)
                    number: adapter.delay; minimum: -10000; maximum: 10000; step: 50; suffix: " ms"
                    onEdited: value => adapter.setDelay(value)
                }
                NumberSetting {
                    label: qsTr("Subtitle Speed"); height: VLCStyle.dp(90)
                    number: adapter.fps * 1000; minimum: 0; maximum: 100000; step: 200; suffix: " fps"; divisor: 1000; decimals: 3; zeroText: qsTr("Auto")
                    onEdited: value => adapter.setFPS(value / 1000)
                }
                NumberSetting {
                    label: adapter.durationAvailable && adapter.durationMode === 0 ? qsTr("Subtitle Duration Extension") : qsTr("Subtitle Duration Factor")
                    height: VLCStyle.dp(90)
                    // Disabled for v0.1: the subsdelay filter trips an engine assertion at the
                    // next cue (vout_subpictures.c spu_PutSubpicture); see DESIGN-DECISIONS.
                    enabled: false
                    number: adapter.duration * 1000; minimum: 0; maximum: 20000; step: 200; divisor: 1000; decimals: 3
                    suffix: adapter.durationAvailable && adapter.durationMode === 0 ? " s" : ""
                    onEdited: value => adapter.setDuration(value / 1000)
                }
                Text {
                    x: VLCStyle.dp(24); width: parent.width - VLCStyle.dp(48)
                    bottomPadding: VLCStyle.dp(20); color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(12); wrapMode: Text.Wrap
                    text: qsTr("Speed affects associated subtitle files, not embedded tracks; 0 selects Auto. Duration extension is unavailable in this version. Appearance and saved defaults are available in a later release.")
                }
            }
        }
    }

    component SectionLabel: Text {
        width: body.width - VLCStyle.dp(48); x: VLCStyle.dp(24); height: VLCStyle.dp(39)
        topPadding: VLCStyle.dp(20); font.pixelSize: VLCStyle.dp(16); color: theme.fg.primary
    }
    component SettingRow: Item {
        id: settingRow
        property string label
        property real controlCenter: height / 2
        default property alias controls: right.data
        width: body.width
        Text { x: VLCStyle.dp(24); width: Math.max(0, right.x - x - VLCStyle.dp(8)); y: settingRow.controlCenter - height / 2; text: parent.label; elide: Text.ElideRight; color: theme.fg.secondary; font.pixelSize: VLCStyle.dp(14) }
        Row { id: right; property string label: settingRow.label; anchors.right: parent.right; anchors.rightMargin: VLCStyle.dp(20); y: settingRow.controlCenter - height / 2 }
    }
    // Plain Templates popup: no Canvas/QtQuick.Effects dependency, including
    // when opened on the static-contrib Qt builds.
    component ModelChoice: T.ComboBox {
        id: choice
        property string emptyText
        implicitHeight: VLCStyle.dp(48)
        padding: VLCStyle.dp(16)
        font.pixelSize: VLCStyle.dp(14)
        readonly property ColorContext colorContext: ColorContext { id: choiceTheme; colorSet: ColorContext.ComboBox; enabled: choice.enabled; hovered: choice.hovered; focused: choice.activeFocus }
        textRole: "display"
        background: Rectangle {
            radius: VLCStyle.dp(6); color: choiceTheme.bg.primary
            border.width: VLCStyle.dp(2); border.color: choice.visualFocus ? VLCStyle.accentForeground(VLCStyle.palette) : theme.bg.secondary
        }
        contentItem: Text { text: choice.displayText; font: choice.font; color: choiceTheme.fg.primary; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; rightPadding: VLCStyle.dp(24) }
        indicator: Widgets.IconLabel { x: choice.width - width - VLCStyle.dp(16); y: (choice.height - height) / 2; width: VLCStyle.dp(24); height: width; font.pixelSize: width; text: VLCIcons.expand; color: choiceTheme.fg.primary }
        delegate: T.ItemDelegate {
            required property int index
            width: choice.popup.availableWidth; height: VLCStyle.dp(44)
            highlighted: choice.highlightedIndex === index
            contentItem: Text { text: choice.textAt(index) || choice.emptyText; color: choiceTheme.fg.primary; font: choice.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            background: Rectangle { radius: VLCStyle.dp(4); color: parent.highlighted ? choiceTheme.bg.secondary : choiceTheme.bg.primary }
        }
        popup: T.Popup {
            y: choice.height + VLCStyle.dp(4); width: Math.max(choice.width, VLCStyle.dp(220)); padding: VLCStyle.dp(2)
            implicitHeight: Math.min(contentItem.contentHeight + padding * 2, VLCStyle.dp(350))
            background: Rectangle { radius: VLCStyle.dp(6); color: choiceTheme.bg.primary }
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight; model: choice.popup.visible ? choice.delegateModel : null
                currentIndex: choice.highlightedIndex
                C.ScrollBar.vertical: Widgets.ScrollBarExt { }
            }
        }
        Navigation.upAction: () => root.advanceFocus(choice, false)
        Navigation.downAction: () => root.advanceFocus(choice, true)
        Keys.priority: Keys.AfterItem
        Keys.onPressed: event => choice.Navigation.defaultKeyAction(event)
        displayText: currentIndex < 0 || currentText === "" ? emptyText : currentText
        Navigation.parentItem: root
        activeFocusOnTab: true
        onActiveFocusChanged: if (activeFocus) root.reveal(this)
        function sync() {
            currentIndex = -1
            for (let row = 0; row < count; ++row) {
                if (model.data(model.index(row, 0), Qt.CheckStateRole)) { currentIndex = row; break }
            }
        }
        onActivated: row => adapter.select(model, row)
        onCountChanged: sync()
        Component.onCompleted: sync()
        Connections {
            target: choice.model
            function onDataChanged() { choice.sync() }
            function onModelReset() { choice.sync() }
            function onRowsInserted() { choice.sync() }
            function onRowsRemoved() { choice.sync() }
        }
    }
    component NumberSetting: Item {
        id: setting
        property string label
        property real number
        property int minimum
        property int maximum
        property int step: 1
        property int sliderStep: step
        property int divisor: 1
        property int decimals: 0
        property string suffix: ""
        property string zeroText: ""
        property var presets: []
        property alias editor: field
        property int headerHeight: 56
        property int fieldHeight: 36
        property int sliderOffset: 61
        signal edited(int value)
        width: body.width
        enabled: adapter.started
        SettingRow {
            label: setting.label; height: VLCStyle.dp(setting.headerHeight)
            Widgets.SpinBoxExt {
                id: field
                activeFocusOnTab: true
                Navigation.upAction: () => root.advanceFocus(field, false)
                Navigation.downAction: () => root.advanceFocus(field, true)
                width: VLCStyle.dp(140); height: VLCStyle.dp(setting.fieldHeight)
                from: setting.minimum; to: setting.maximum; stepSize: setting.step
                value: Math.round(setting.number)
                editable: true
                textFromValue: (value, locale) => value === 0 && setting.zeroText ? setting.zeroText : Number(value / setting.divisor).toLocaleString(locale, 'f', setting.decimals) + setting.suffix
                valueFromText: (text, locale) => setting.zeroText && text.trim() === setting.zeroText ? 0 : Math.round(Number.fromLocaleString(locale, text.replace(setting.suffix, "")) * setting.divisor)
                validator: DoubleValidator { bottom: setting.minimum / setting.divisor; top: setting.maximum / setting.divisor; decimals: setting.decimals }
                onValueModified: setting.edited(value)
                Accessible.name: setting.label
                Navigation.parentItem: root
                onActiveFocusChanged: if (activeFocus) root.reveal(setting)
            }
        }
        Widgets.SliderExt {
            id: slider
            x: VLCStyle.dp(20); y: VLCStyle.dp(setting.sliderOffset); width: parent.width - VLCStyle.dp(40); height: VLCStyle.dp(24)
            from: setting.minimum; to: setting.maximum; stepSize: setting.sliderStep
            snapMode: setting.presets.length > 0 ? T.Slider.SnapAlways : T.Slider.NoSnap
            value: setting.number; handleWidth: VLCStyle.dp(8)
            onMoved: setting.edited(Math.round(value))
            Repeater {
                model: setting.presets.length
                Rectangle {
                    required property int index
                    visible: index > 0 && index < setting.presets.length - 1
                    x: (2.195 + index * 355.61 / 7) * slider.width / 360 - VLCStyle.dp(1)
                    y: VLCStyle.dp(9); width: VLCStyle.dp(2); height: VLCStyle.dp(6); radius: VLCStyle.dp(1)
                    color: theme.palette?.bgAccentDark ?? VLCStyle.palette.bgAccentDark
                    z: -0.5
                }
            }
            Accessible.name: setting.label
            Navigation.parentItem: root
            Navigation.upAction: () => root.advanceFocus(slider, false)
            Navigation.downAction: () => root.advanceFocus(slider, true)
            activeFocusOnTab: true
            onActiveFocusChanged: if (activeFocus) root.reveal(setting)
            toolTipTextProvider: value => Number(value / setting.divisor).toFixed(setting.decimals) + setting.suffix
        }
        Item {
            x: VLCStyle.dp(20); y: VLCStyle.dp(92); width: parent.width - VLCStyle.dp(40); height: VLCStyle.dp(24)
            visible: setting.presets.length > 0
            Repeater {
                model: setting.presets
                T.Button {
                    id: preset
                    required property int modelData
                    required property int index
                    x: [2, 42, 92, 142, 192, 242, 292, 332][index] * parent.width / 360
                    width: VLCStyle.dp(26); height: VLCStyle.dp(24)
                    activeFocusOnTab: true
                    text: String(modelData / setting.divisor)
                    contentItem: Text { text: preset.text; font.pixelSize: VLCStyle.dp(12); color: theme.fg.secondary; verticalAlignment: Text.AlignTop; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "transparent"; radius: VLCStyle.dp(4); border.width: preset.visualFocus ? VLCStyle.dp(1) : 0; border.color: theme.accent }
                    onClicked: setting.edited(modelData)
                    Accessible.name: qsTr("Speed %1x").arg(text)
                    Navigation.parentItem: root
                    Navigation.upAction: () => slider.forceActiveFocus(Qt.BacktabFocusReason)
                    Navigation.downAction: () => root.advanceFocus(preset, true)
                    Keys.onPressed: event => preset.Navigation.defaultKeyAction(event)
                }
            }
        }
        Row {
            visible: setting.minimum < 0
            x: VLCStyle.dp(20); y: VLCStyle.dp(92); width: parent.width - VLCStyle.dp(40)
            Repeater {
                model: [setting.minimum + " ms", "0 ms", setting.maximum + " ms"]
                Text { required property string modelData; required property int index; width: parent.width / 3; text: modelData; font.pixelSize: VLCStyle.dp(12); color: theme.fg.secondary; horizontalAlignment: index === 0 ? Text.AlignLeft : index === 1 ? Text.AlignHCenter : Text.AlignRight }
            }
        }
    }
}
