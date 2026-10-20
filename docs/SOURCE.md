# Source identity

| Item | Revision |
| :--- | :--- |
| VLC 4 upstream base | `a5538c557d276378d5ba0f8f8010a8b87eaa6f78` |
| Modified player source imported for v0.1 | `8e4e3e86d18a128d333d9016666eace138700929` |
| Upstream | https://code.videolan.org/videolan/vlc |
| Public project | https://github.com/ShikherVerma/vlc-juxtopposed |

This repository is a GitHub fork of `videolan/vlc`. The main branch preserves the original VLC ancestry and all 41 project implementation commits after the pinned upstream base. Presentation commits add the project README, public docs and screenshots. `docs/UPSTREAM-README.md` preserves VLC's original README. Private orchestration history, testing profiles and raw machine logs are not imported.

The redesign primarily changes `modules/gui/qt/`. Additional changes support platform packaging, macOS startup/deployment and embedded-video geometry, an associated-subtitle-FPS symbol export, and native Windows plugin-cache packaging. Temporary Windows core shutdown instrumentation remains an unmerged diagnostic lane and is not included in this public snapshot.

The macOS candidate tested at this product pin is Developer ID signed/notarized, with SHA256 `6550c52dd9641d9517820ea39f698c29949791b325f1d6af6e32ffc22aa97cd5`. It is not attached as a binary download to v0.1. Historical Windows and Linux artifacts were built at earlier product pins and are not presented as matching this tag.
