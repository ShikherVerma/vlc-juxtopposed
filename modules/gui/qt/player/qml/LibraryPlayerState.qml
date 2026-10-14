/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
import QtQml

// Compact browsing shares the full player's surface. Only the full library
// owns a docked player, including paused and audio-only inputs.
QtObject {
    property bool started: false
    property bool compact: false
    property bool embeddedVideo: false
    property bool pipAvailable: false
    readonly property bool docked: started && !compact && (!embeddedVideo || pipAvailable)
}
