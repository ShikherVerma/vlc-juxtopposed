/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Player

T.Pane {
    id: root

    property bool paintOnly: false

    // Source component 244:184 defines these authored level widths.
    function levelWidth(volume) {
        const levels = [0, .1, .2, .3, .4, .5, .6, .7, .8, .9, 1, 1.5, 2]
        const widths = [1, 7, 14, 20, 26, 33, 41, 48, 53, 58, 63, 75, 88]
        for (let i = 1; i < levels.length; ++i)
            if (volume <= levels[i])
                return widths[i-1] + (widths[i] - widths[i-1]) * (volume - levels[i-1]) / (levels[i] - levels[i-1])
        return 88
    }

    function volumeAt(x) {
        const levels = [0, .1, .2, .3, .4, .5, .6, .7, .8, .9, 1, 1.5, 2]
        const widths = [1, 7, 14, 20, 26, 33, 41, 48, 53, 58, 63, 75, 88]
        for (let i = 1; i < widths.length; ++i)
            if (x <= widths[i])
                return Math.max(0, levels[i-1] + (levels[i] - levels[i-1]) * (x - widths[i-1]) / (widths[i] - widths[i-1]))
        return 2
    }

    readonly property var _player: paintOnly ? ({ muted: false, volume: .5 }) : Player
    readonly property real _displayVolume: _player.volume >= 0 ? _player.volume : 1
    readonly property int _maxvol: MainCtx.maxVolume
    readonly property real _fullvolpos: 100 / _maxvol
    readonly property real _maxvolpos: _maxvol / 100

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentWidth: volumeWidget.implicitWidth
    contentHeight: volumeWidget.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    RowLayout {
        id: volumeWidget

        anchors.fill: parent
        spacing: VLCStyle.dp(8, VLCStyle.scale)

        Widgets.IconToolButton{
            id: volumeBtn

            padding: 0
            leftPadding: 0
            rightPadding: 0
            implicitWidth: VLCStyle.dp(24, VLCStyle.scale)
            implicitHeight: VLCStyle.dp(28, VLCStyle.scale)
            focus: true
            paintOnly: root.paintOnly
            text: {
                if (_player.muted)
                    return VLCIcons.volume_muted
                else if (_player.volume === 0)
                    return VLCIcons.volume_zero
                else
                    return VLCIcons.volume_high
            }
            description: qsTr("Mute")

            AccessibleCompat.id: "muteButton"

            onClicked: Player.muted = !Player.muted

            Accessible.onIncreaseAction: {
                Player.muted = false
                Player.setVolumeUp()
            }

            Accessible.onDecreaseAction: {
                Player.muted = false
                Player.setVolumeDown()
            }

            Navigation.parentItem: root
            Navigation.rightItem: volControl
        }

        Widgets.SliderExt {
            id: volControl

            // FIXME: use VLCStyle
            implicitWidth: VLCStyle.dp(88, VLCStyle.scale)
            implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

            // WARNING: Directly setting the insets break handle and background positioning.
            Binding on topInset {
                when: MainCtx.usingTouch
                value: VLCStyle.touchHandlerMargin
            }
            Binding on bottomInset {
                when: MainCtx.usingTouch
                value: VLCStyle.touchHandlerMargin
            }

            Layout.fillHeight: true
            Layout.margins: 0

            property bool _inhibitPlayerVolumeUpdate: false

            // FIXME: Currently we are not updating the ShiftModifier status while dragging. This
            //        could be fixed with a custom Slider based on a MouseArea.
            property bool _shiftPressed: false

            property real _clamp: 0.01

            property bool _keyPressed: false

            property bool filterEvents: false

            stepSize: 0.05

            from: 0
            to: root._maxvolpos
            opacity: _player.muted ? 0.5 : 1

            enabled: !root.paintOnly // disables event handling depending on this

            toolTipTextProvider: function (value) {
                // the real value i.e 'Player.volume' bounds can be different
                // from bounds of this widget and we want to show the current
                // volume here, so directly use Player.volume instead of "value"

                return Player.volume < 0 ? "—" : Math.round(Player.volume * 100) + " %"
            }

            Accessible.name: qsTr("Volume")
            AccessibleCompat.id: "volumeSlider"

            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true

                    _keyPressed = true
                } else {
                    Navigation.defaultKeyAction(event)
                }
            }

            Keys.onReleased: (event) => {
                if (_keyPressed === false)
                    return

                _keyPressed = false

                if (KeyHelper.matchOk(event)) {
                    Player.muted = !Player.muted
                }
            }

            function _syncVolumeWithPlayer() {
                volControl._inhibitPlayerVolumeUpdate = true
                volControl.value = root._displayVolume
                volControl._inhibitPlayerVolumeUpdate = false
            }

            function _adjustPlayerVolume() {
                Player.muted = false

                let value = volControl.value

                // NOTE: We are clamping the value to make it easier to restore the default volume.
                if (_shiftPressed === false) {
                    if (Math.abs(value - 1.0) < _clamp)
                        value = 1.0
                    else
                        _clamp = 0.01
                }

                Player.volume = value

                volControl.value = value
            }

            Component.onCompleted: {
                root.paintOnlyChanged.connect(_syncVolumeWithPlayer)
                volControl._syncVolumeWithPlayer()
            }

            Connections {
                target: Player
                enabled: !paintOnly

                function onVolumeChanged() { volControl._syncVolumeWithPlayer() }
            }

            function updateTooltip() {
                if (!root.paintOnly && (sliderMouseArea.containsMouse || sliderMouseArea.pressed || visualFocus))
                    volumeTip.open()
                else
                    volumeTip.close()
            }

            onVisualFocusChanged: updateTooltip()

            // This control has the authored numeric popup, not the shared arrow tooltip.
            Widgets.PointingToolTipAttached.visible: false

            T.ToolTip {
                id: volumeTip
                delay: VLCStyle.delayToolTipAppear
                // Native video children cover in-scene popups; use a tooltip window.
                Component.onCompleted: {
                    if (volumeTip.popupType !== undefined)
                        volumeTip.popupType = 1 // Popup.Window, Qt 6.8+
                }
                text: Player.volume < 0 ? "—" : Math.round(Player.volume * 100) + " %"
                width: VLCStyle.dp(121, VLCStyle.scale)
                height: VLCStyle.dp(37, VLCStyle.scale)
                x: root.levelWidth(root._displayVolume) * volControl.width / 88 - width / 2
                y: -height - VLCStyle.dp(10, VLCStyle.scale)
                padding: VLCStyle.dp(10, VLCStyle.scale)
                font.family: VLCStyle.fontFamily
                font.pixelSize: VLCStyle.dp(14, VLCStyle.scale)

                readonly property ColorContext colorContext: ColorContext {
                    id: tooltipTheme
                    palette: VLCStyle.palette
                    colorSet: ColorContext.Tooltip
                }

                background: Rectangle {
                    radius: VLCStyle.control_radius
                    color: tooltipTheme.bg.secondary
                }
                contentItem: T.Label {
                    text: volumeTip.text
                    font: volumeTip.font
                    color: tooltipTheme.fg.secondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Navigation.leftItem: volumeBtn
            Navigation.parentItem: root

            Keys.onUpPressed: (event) => {
                Player.muted = false
                Player.setVolumeUp()
            }

            Keys.onDownPressed: (event) => {
                Player.muted = false
                Player.setVolumeDown()
            }

            Keys.priority: Keys.BeforeItem

            onValueChanged: {
                if (paintOnly)
                    return

                if (!volControl._inhibitPlayerVolumeUpdate) {
                    if (filterEvents) {
                        Qt.callLater(volControl._adjustPlayerVolume)
                    } else {
                        volControl._adjustPlayerVolume()
                        filterEvents = true
                    }
                }
            }

            handle: Item {
                x: root.levelWidth(volControl.value) * volControl.width / 88
                implicitWidth: 0
                implicitHeight: VLCStyle.dp(28, VLCStyle.scale)
            }

            background: Widgets.VolumeCone {
                anchors.fill: parent
                fillWidth: root.levelWidth(root._displayVolume)
                wellColor: volControl.enabled ? volControl.colorContext.bg.neutral : volControl.colorContext.bg.primary
                fillColor: volControl.colorContext.bg.secondary
                useGradient: volControl.enabled
                dark: volControl.colorContext.palette?.isDark ?? VLCStyle.palette.isDark
            }

            MouseArea {
                id: sliderMouseArea

                anchors.fill: parent

                acceptedButtons: (Qt.LeftButton | Qt.RightButton)
                hoverEnabled: true
                onContainsMouseChanged: volControl.updateTooltip()
                onPressedChanged: volControl.updateTooltip()

                onPressed: (mouse) => {
                    if (root.paintOnly) {
                        mouse.accepted = true
                        return
                    }

                    volControl.forceActiveFocus(Qt.MouseFocusReason)
                    volControl._shiftPressed = (mouse.modifiers === Qt.ShiftModifier)

                    if (mouse.button === Qt.LeftButton) {
                        volControl.value = Math.min(root._maxvolpos, root.volumeAt(mouse.x * 88 / width))
                        return
                    }

                    volControl.filterEvents = false
                    adjustVolume(mouse)
                }

                onPositionChanged: (mouse) => {
                    if (mouse.buttons & Qt.RightButton)
                        adjustVolume(mouse)
                    else if (mouse.buttons & Qt.LeftButton)
                        volControl.value = Math.min(root._maxvolpos, root.volumeAt(mouse.x * 88 / width))
                }

                onWheel: (wheel) => {

                    if ((Math.abs(wheel.pixelDelta.x) % 120 > 0) || (Math.abs(wheel.pixelDelta.y) % 120 > 0)) {
                        let delta = 0
                        if (Math.abs(wheel.pixelDelta.x) > Math.abs(wheel.pixelDelta.y))
                            delta = wheel.pixelDelta.x
                        else
                            delta = wheel.pixelDelta.y

                        if (wheel.inverted)
                            delta = -delta
                        volControl.value += 0.001 * delta
                    }
                    else
                        wheelToVLC.qmlWheelEvent(wheel)

                    wheel.accepted = true
                }

                function adjustVolume(mouse) {
                    mouse.accepted = true

                    const pos = mouse.x * root._maxvolpos / width

                    if (pos < 0.25)
                        volControl.value = 0
                    else if (pos < 0.75)
                        volControl.value = 0.5
                    else if (pos < 1.125)
                        volControl.value = 1
                    else
                        volControl.value = 1.25
                }

                WheelToVLCConverter {
                    id: wheelToVLC

                    function handle(steps: int) {
                        if (steps > 0)
                            Player.setVolumeUp(steps)
                        else
                            Player.setVolumeDown(-steps)
                    }

                    Component.onCompleted: {
                        wheelUpDown.connect(wheelToVLC.handle)
                        wheelLeftRight.connect(wheelToVLC.handle)
                    }
                }
            }
        }
    }
}
