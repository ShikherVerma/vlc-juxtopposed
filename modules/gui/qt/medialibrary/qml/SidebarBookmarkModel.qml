/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
import VLC.MediaLibrary
import VLC.Player

// Loaded only when MainCtx.mediaLibraryAvailable, like the seek-bar bookmarks.
MLBookmarkModel {
    player: Player
    ml: MediaLib
}
