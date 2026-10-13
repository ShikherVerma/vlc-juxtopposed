/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
import QtQml
import VLC.Player
import VLC.Dialogs

// Presentation-only adapter. All playback/track/filter operations stay in VLC's
// controller/models and the shared extended-panel duration implementation.
QtObject {
    readonly property bool started: Player.isStarted
    readonly property real rate: Player.rate
    readonly property int delay: Player.subtitleDelayMS
    readonly property real fps: Player.subtitleFPS
    readonly property real duration: Player.subtitleDurationFactor
    readonly property int durationMode: Player.subtitleDurationMode
    readonly property bool durationAvailable: Player.subtitleDurationAvailable
    readonly property bool statisticsAvailable: Player.statisticsAvailable
    readonly property var statistics: Player.statistics
    readonly property var tracks: Player.subtitleTracks
    readonly property var devices: Player.audioDevices
    readonly property var visualizations: Player.audioVisualization
    readonly property bool visualizationsAvailable: Player.hasAudioVisualization

    function setRate(value) { if (started) Player.rate = value }
    function setDelay(value) { if (started) Player.subtitleDelayMS = value }
    function setFPS(value) { if (started) Player.subtitleFPS = value }
    function setDuration(value) { if (started && durationAvailable) Player.subtitleDurationFactor = value }
    function addSubtitle() { DialogsProvider.loadSubtitlesFile() }
    function select(model, row) {
        if (row >= 0) model.setData(model.index(row, 0), true, Qt.CheckStateRole)
    }
    function disableSubtitles() {
        for (let row = 0; row < tracks.rowCount(); ++row)
            if (tracks.data(tracks.index(row, 0), Qt.CheckStateRole))
                tracks.setData(tracks.index(row, 0), false, Qt.CheckStateRole)
    }
}
