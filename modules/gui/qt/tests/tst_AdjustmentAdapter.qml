/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
// Runner supplies only the singleton boundaries; the adapter is production QML.
import QtQuick
import QtTest
import VLC.Player
import VLC.Dialogs

TestCase {
    name: "AdjustmentAdapter"
    AdjustmentAdapter { id: adapter }
    function init() {
        Player.isStarted = true
        Player.rate = 1
        Player.subtitleDelayMS = 0
        Player.subtitleFPS = 1
        Player.subtitleDurationFactor = 0
        Player.subtitleDurationAvailable = true
        DialogsProvider.subtitleRequests = 0
    }
    function test_liveSettersAndReadbacks() {
        adapter.setRate(1.25)
        adapter.setDelay(-450)
        adapter.setFPS(23.976)
        adapter.setDuration(2.5)
        compare(Player.rate, 1.25)
        compare(adapter.rate, 1.25)
        compare(adapter.delay, -450)
        fuzzyCompare(adapter.fps, 23.976, 0.001)
        compare(adapter.duration, 2.5)
        Player.subtitleDelayMS = 350
        compare(adapter.delay, 350)
    }
    function test_automaticSubtitleTiming() {
        adapter.setFPS(0)
        compare(Player.subtitleFPS, 0)
        compare(adapter.fps, 0)
    }
    function test_noInputDoesNotWrite() {
        Player.isStarted = false
        adapter.setRate(1.5)
        adapter.setDelay(400)
        adapter.setFPS(25)
        adapter.setDuration(3)
        compare(Player.rate, 1)
        compare(Player.subtitleDelayMS, 0)
        compare(Player.subtitleFPS, 1)
        compare(Player.subtitleDurationFactor, 0)
    }
    function test_missingDurationModule() {
        Player.subtitleDurationAvailable = false
        adapter.setDuration(5)
        compare(Player.subtitleDurationFactor, 0)
    }
    function test_existingSubtitleDialogAction() {
        adapter.addSubtitle()
        compare(DialogsProvider.subtitleRequests, 1)
    }
    function test_selectionUsesModelCheckRole() {
        const writes = []
        const model = { index: row => row, setData: (row, value, role) => writes.push([row, value, role]) }
        adapter.select(model, -1)
        compare(writes.length, 0)
        adapter.select(model, 2)
        compare(writes.length, 1)
        compare(writes[0][0], 2)
        compare(writes[0][1], true)
        compare(writes[0][2], Qt.CheckStateRole)
    }
}
