/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Player
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

FocusScope {
    id: resumePanel

    property int maxWidth
    readonly property real dp: VLCStyle.scale
    readonly property bool small: maxWidth < naturalWidth
    readonly property real naturalWidth: label.implicitWidth + VLCStyle.dp(60 + 20 + 10 + 10, dp)
                                        + continueBtn.implicitWidth + closeBtn.width
    readonly property bool hasSavedTime: Player.restorePlaybackPosition > 0
                                        && Player.restorePlaybackPosition <= 1
                                        && Player.length.valid() && Player.length.toSeconds() > 0
    readonly property var savedDuration: Player.length.scale(Player.restorePlaybackPosition)
    readonly property string savedTime: hasSavedTime
        ? (savedDuration.isSubHour() ? "00:" : "") + savedDuration.formatHMS(0) : ""

    implicitWidth: Math.min(maxWidth, naturalWidth)
    implicitHeight: small ? label.height + VLCStyle.dp(52, dp) : VLCStyle.dp(52, dp)
    width: implicitWidth
    height: implicitHeight
    // Keep offer lifetime separate from inherited item visibility: view teardown
    // can hide ancestors before Component.onDestruction acknowledges the offer.
    property bool _offered: false
    visible: _offered
    enabled: visible

    signal hidden()

    function showResumePanel() {
        _offered = true
        resumeTimeout.start()
        Qt.callLater(() => continueBtn.forceActiveFocus(Qt.TabFocusReason))
    }

    function hideResumePanel() {
        if (!_offered) return
        resumeTimeout.stop()
        _offered = false
        Player.acknowledgeRestoreCallback()
        hidden()
    }

    Timer {
        id: resumeTimeout
        interval: 10000
        onTriggered: resumePanel.hideResumePanel()
    }

    Connections {
        target: Player
        function onPlaybackRestoreQueried() {
            if (Player.canRestorePlayback) resumePanel.showResumePanel()
            else resumePanel.hideResumePanel()
        }
    }

    Component.onCompleted: {
        if (Player.canRestorePlayback) showResumePanel()
    }
    // Hide UI/library replacement dismisses this notification, not the engine
    // dialog provider. Do not restart the offer when a new Player view is built.
    Component.onDestruction: {
        if (_offered) Player.acknowledgeRestoreCallback()
    }

    // Draw in Player's overlay alongside its controls, not in the shared window
    // Popup overlay (which can be occluded by fullscreen video composition).
    Rectangle {
        anchors.fill: parent
        color: VLCStyle.popupTint(true)
        radius: VLCStyle.dp(16, resumePanel.dp)
    }

    FocusScope {
        anchors.fill: parent
        id: content
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Resume dialog")
        Navigation.parentItem: resumePanel
        Navigation.cancelAction: resumePanel.hideResumePanel

        T.Label {
            id: label
            x: VLCStyle.dp(20, resumePanel.dp)
            y: resumePanel.small ? VLCStyle.dp(10, resumePanel.dp) : (parent.height - height) / 2
            width: resumePanel.small ? parent.width - 2 * x : implicitWidth
            color: "#c6c6c6" // Dark/FG/2 in both authored instances
            font.family: VLCStyle.fontFamily
            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.Normal
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            text: resumePanel.hasSavedTime
                  ? qsTr("You’ve watched till %1 already.").arg("<span style='font-weight:600'>" + resumePanel.savedTime + "</span>")
                  : qsTr("Continue where you left off?")
        }

        Widgets.ButtonExt {
            id: continueBtn
            x: resumePanel.small ? VLCStyle.dp(20, resumePanel.dp)
                                : label.x + label.width + VLCStyle.dp(60, resumePanel.dp)
            y: resumePanel.small ? label.y + label.height + VLCStyle.dp(10, resumePanel.dp)
                                : VLCStyle.dp(10, resumePanel.dp)
            height: VLCStyle.dp(32, resumePanel.dp)
            radius: VLCStyle.dp(6, resumePanel.dp)
            text: qsTr("Continue Watching")
            AccessibleCompat.id: "resumeContinueButton"
            color: "#161616" // FG/B1, as in the repeated dark/light instances
            background: Rectangle {
                radius: continueBtn.radius
                color: continueBtn.down ? VLCStyle.darkPalette.accentDim
                                        : continueBtn.hovered ? VLCStyle.darkPalette.accentBright
                                                              : VLCStyle.darkPalette.accentMain
                border.width: continueBtn.visualFocus ? 1 : 0
                border.color: continueBtn.color
            }
            contentItem: T.Label {
                text: continueBtn.text
                color: continueBtn.color
                font.family: VLCStyle.fontFamily
                font.pixelSize: VLCStyle.fontSize_normal
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Accessible.ignored: true
            }
            onClicked: {
                Player.restorePlaybackPos()
                resumePanel.hideResumePanel()
            }
            Navigation.parentItem: content
            Navigation.rightItem: closeBtn
            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event)) event.accepted = true
                else continueBtn.Navigation.defaultKeyAction(event)
            }
            Keys.onReleased: (event) => {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true
                    continueBtn.clicked()
                }
            }
        }

        Widgets.ButtonExt {
            id: closeBtn
            x: continueBtn.x + continueBtn.width + VLCStyle.dp(10, resumePanel.dp)
            y: continueBtn.y
            width: VLCStyle.dp(44, resumePanel.dp)
            height: continueBtn.height
            text: qsTr("Dismiss")
            showText: false
            iconTxt: VLCIcons.close
            iconSize: VLCStyle.dp(24, resumePanel.dp)
            color: "#8f8f8f"
            background: Rectangle {
                radius: VLCStyle.dp(6, resumePanel.dp)
                color: closeBtn.down ? "#373737" : closeBtn.hovered ? "#222222" : "transparent"
                border.width: closeBtn.visualFocus ? 1 : 0
                border.color: VLCStyle.darkPalette.accentMain
            }
            AccessibleCompat.id: "resumeDismissButton"
            onClicked: resumePanel.hideResumePanel()
            Navigation.parentItem: content
            Navigation.leftItem: continueBtn
            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event)) event.accepted = true
                else closeBtn.Navigation.defaultKeyAction(event)
            }
            Keys.onReleased: (event) => {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true
                    closeBtn.clicked()
                }
            }
        }
    }
}
