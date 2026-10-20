# From the video to the desktop

Juxtopposed's [VLC redesign video](https://www.youtube.com/watch?v=ERFeOgqk324) and [Figma design](https://www.figma.com/community/file/1691465999134370980/vlc-redesign), translated into a shared Qt/QML interface on VLC 4.

**Built** means the redesigned feature is implemented in the shared desktop source. **Core** means its main controls are implemented, not the entire video concept. **Planned** means the redesigned workflow is not implemented. These describe implementation, not identical runtime verification on every OS. See [platform testing](STATUS.md).

| Feature | macOS | Linux | Windows |
| :--- | :---: | :---: | :---: |
| Dark, light and system themes | Built | Built | Built |
| Accent colors, icons and typography | Built | Built | Built |
| Player bar, seek bar and cone volume | Built | Built | Built |
| Hide the interface | Built | Built | Built |
| Fullscreen top and bottom bars | Built | Built | Built |
| Continue watching prompt | Built | Built | Built |
| Main menu and right-click menu | Built | Built | Built |
| Audio and subtitle track selection | Built | Built | Built |
| Queue sidebar | Built | Built | Built |
| Chapters sidebar | Built | Built | Built |
| Add, rename and remove bookmarks | Built | Built | Built |
| Playback speed and quick adjustments | Built | Built | Built |
| Audio and subtitle delay | Built | Built | Built |
| Subtitle font, color and position | Core | Core | Core |
| Video and audio effects | Core | Core | Core |
| Equalizer, compressor and effect detail panels | Planned | Planned | Planned |
| Subtitle duration adjustment | Disabled | Disabled | Disabled |
| Home and continue-watching sections | Built | Built | Built |
| Video folders and breadcrumbs | Built | Built | Built |
| Grid, list, search and sorting | Built | Built | Built |
| Music, artists, albums and genres | Built | Built | Built |
| Network streams and recent files | Built | Built | Built |
| Saved playlists and playlist search | Built | Built | Built |
| Custom playlist artwork and pinned collections | Planned | Planned | Planned |
| Compact library while watching | Built | Built | Built |
| Docked mini-player | Built | Built | Built |
| Searchable settings shell | Built | Built | Built |
| General and Interface settings | Built | Built | Built |
| Playback, Audio and Subtitles settings | Built | Built | Built |
| Remaining eight redesigned settings pages | Planned | Planned | Planned |
| Searchable redesigned shortcut editor | Planned | Planned | Planned |
| First-run privacy and media-folder setup | Built | Built | Built |
| Full welcome, theme and shortcut tour | Planned | Planned | Planned |
| Drag-and-drop player customization | Planned | Planned | Planned |
| Movies, Shows, Music and Work profiles | Planned | Planned | Planned |
| Folder and show playback presets | Planned | Planned | Planned |
| Batch subtitle and external audio matching | Planned | Planned | Planned |
| Online subtitles and subtitle links | Planned | Planned | Planned |
| Seek-bar thumbnail previews | Planned | Planned | Planned |
| Whole-track audio waveform | Planned | Planned | Planned |
| Subtitle timeline and drag-to-sync | Planned | Planned | Planned |
| Floating picture-in-picture | Planned | Planned | Planned |
| Sleep timer and redesigned statistics | Planned | Planned | Planned |
| Shortcut feedback overlay | Core | Core | Core |

The five redesigned settings pages expose **101 controls**. Other VLC options remain reachable through **All settings**. Existing VLC features accessible through original dialogs are not counted as completed redesigns here.

The docked mini-player is implemented. A separate floating PiP window is a different roadmap item. Audio device selection is session-only; availability depends on the output backend. Subtitle Duration is disabled because of an upstream filter assertion. The shortcut feedback work covers selected actions such as bookmark creation, not every keyboard shortcut.
