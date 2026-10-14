/* SPDX-License-Identifier: GPL-2.0-or-later */
import QtQuick
import QtTest
import "../player/qml" as Production
TestCase {
    name: "LibraryPlayerState"
    Production.LibraryPlayerState { id: state }
    function test_docked_data() {
        return [
            { tag: "stopped", started: false, compact: false, embedded: true, pip: true, expected: false },
            { tag: "video-playing-or-paused", started: true, compact: false, embedded: true, pip: true, expected: true },
            { tag: "compact-single-surface", started: true, compact: true, embedded: true, pip: true, expected: false },
            { tag: "audio", started: true, compact: false, embedded: false, pip: false, expected: true },
            { tag: "audio-compact", started: true, compact: true, embedded: false, pip: false, expected: false },
            { tag: "unsupported-video-pip", started: true, compact: false, embedded: true, pip: false, expected: false }
        ]
    }
    function test_docked(row) {
        state.started = row.started; state.compact = row.compact
        state.embeddedVideo = row.embedded; state.pipAvailable = row.pip
        compare(state.docked, row.expected)
    }
    function test_lifecycle() {
        state.started = true; state.compact = false
        state.embeddedVideo = true; state.pipAvailable = true
        verify(state.docked)
        state.started = false; verify(!state.docked)
        state.started = true; verify(state.docked)
        state.compact = true; verify(!state.docked)
        state.compact = false; verify(state.docked)
    }
}
