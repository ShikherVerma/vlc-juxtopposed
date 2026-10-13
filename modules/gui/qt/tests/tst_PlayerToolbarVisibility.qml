// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
// Production FSM, with engine/window singleton fixtures supplied by the runner.
import QtQuick
import QtQuick.Window
import QtTest
import VLC.Player as PlayerUI

TestCase {
    name: "PlayerToolbarVisibility"
    PlayerUI.PlayerToolbarVisibilityFSM { id: toolbar }

    function init() {
        toolbar.fullscreen = true
        toolbar.topLocks = 0
        toolbar.bottomLocks = 0
        toolbar.pointerAt(540, 1080, 40, 54)
    }

    function test_edges_data() {
        return [
            {tag: "top", y: 0, top: true, bottom: false},
            {tag: "top-inner", y: 39, top: true, bottom: false},
            {tag: "top-outside", y: 40, top: false, bottom: false},
            {tag: "middle", y: 540, top: false, bottom: false},
            {tag: "bottom-outside", y: 1025, top: false, bottom: false},
            {tag: "bottom-inner", y: 1026, top: false, bottom: true},
            {tag: "bottom", y: 1080, top: false, bottom: true}
        ]
    }
    function test_edges(data) {
        toolbar.pointerAt(data.y, 1080, 40, 54)
        compare(toolbar.topVisible, data.top)
        compare(toolbar.bottomVisible, data.bottom)
    }
    function test_independentPopupLocks() {
        toolbar.lockTop(true)
        toolbar.lockTop(true)
        compare(toolbar.topVisible, true)
        compare(toolbar.bottomVisible, false)
        toolbar.lockTop(false)
        compare(toolbar.topVisible, true)
        toolbar.lockTop(false)
        compare(toolbar.topVisible, false)
        toolbar.lockBottom(true)
        compare(toolbar.topVisible, false)
        compare(toolbar.bottomVisible, true)
        toolbar.lockBottom(false)
        toolbar.lockBottom(false)
        compare(toolbar.bottomLocks, 0)
        compare(toolbar.bottomVisible, false)
    }
    function test_leaveIsImmediate() {
        toolbar.pointerAt(5, 1080, 40, 54)
        compare(toolbar.topVisible, true)
        toolbar.pointerAt(540, 1080, 40, 54)
        compare(toolbar.topVisible, false)
        toolbar.pointerAt(1070, 1080, 40, 54)
        compare(toolbar.bottomVisible, true)
        toolbar.pointerAt(540, 1080, 40, 54)
        compare(toolbar.bottomVisible, false)
    }
    function test_windowedUsesExistingFSM() {
        toolbar.fullscreen = false
        compare(toolbar.topVisible, toolbar.isVisible)
        compare(toolbar.bottomVisible, toolbar.isVisible)
        toolbar.fullscreen = true
        compare(toolbar.topEdge, false)
        compare(toolbar.bottomEdge, false)
    }
}
