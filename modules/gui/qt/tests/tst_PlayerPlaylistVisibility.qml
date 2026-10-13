// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
// Exercise the production sidebar lifecycle, with only MainCtx supplied by the runner.
import QtQuick
import QtTest
import VLC.Player
import VLC.MainInterface

TestCase {
    name: "PlayerPlaylistVisibility"
    Component { id: factory; PlayerPlaylistVisibilityFSM {} }
    property var sidebar
    function init() {
        MainCtx.playqueuePanel.docked = true
        MainCtx.playqueuePanel.visible = false
        MainCtx.hasEmbededVideo = true
        sidebar = factory.createObject(this)
        wait(0)
    }
    function cleanup() { sidebar.destroy(); wait(0) }
    function test_toggleWithVideo() {
        compare(sidebar.isPlaylistVisible, false)
        sidebar.togglePlaylistVisibility()
        compare(sidebar.isPlaylistVisible, true)
        compare(MainCtx.playqueuePanel.visible, true)
        sidebar.togglePlaylistVisibility()
        compare(sidebar.isPlaylistVisible, false)
        compare(MainCtx.playqueuePanel.visible, false)
    }
    function test_externalClose() {
        sidebar.togglePlaylistVisibility()
        MainCtx.playqueuePanel.visible = false
        sidebar.updatePlaylistVisible()
        compare(sidebar.isPlaylistVisible, false)
    }
    function test_videoEndsPreservesOpenSidebar() {
        sidebar.togglePlaylistVisibility()
        MainCtx.hasEmbededVideo = false
        sidebar.updateVideoEmbed()
        compare(sidebar.isPlaylistVisible, true)
        sidebar.togglePlaylistVisibility()
        compare(sidebar.isPlaylistVisible, false)
    }
    function test_floatingUsesPanelVisibility() {
        MainCtx.playqueuePanel.docked = false
        sidebar.updatePlaylistDocked()
        sidebar.togglePlaylistVisibility()
        compare(MainCtx.playqueuePanel.visible, true)
        compare(sidebar.isPlaylistVisible, false)
        sidebar.togglePlaylistVisibility()
        compare(MainCtx.playqueuePanel.visible, false)
    }
}
