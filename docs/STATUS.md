# Platform testing

## v1 snapshot

The tag contains the redesigned VLC source, screenshots and project documentation. Installers are not attached to this source preview.

| Platform | What has been exercised | Still to qualify |
| :--- | :--- | :--- |
| Linux x86_64 | Player, library, 101 settings controls, mini-player, themes and repeated quit routes. AppImage tested in Ubuntu 22.04/24.04, Debian 12 and Fedora 41 container userlands. | Final-source AppImage rebuild, physical GPU, Wayland, FUSE and native PipeWire. |
| Windows x64 | Qt runtime, playback, settings and quit tests in the existing VM. Installer plugin cache reduced installation to about 5 to 7 seconds. | Fresh-install startup can take about 140 seconds to advancing video. Two of 60 release quit routes hung after saving settings. Matching final installer/ZIP and latest library/settings coverage. |
| macOS arm64 | Signed/notarized build; default Qt entry, Metal, VideoToolbox, cold/warm file opening and advancing playback. Setup Quit and explicit Finish persistence tested. | Full hands-on controls/settings/library sweep, foreground fullscreen/Spaces/resize, older macOS, Intel and physical audio. Four prior accessibility teardown crashes remain unresolved. |

Current Mac candidate source is `8e4e3e86d18a128d333d9016666eace138700929`. Independent offline checks covered all 5,978 tracked source entries, 390 plugins, 398 arm64 Mach-O files, normal-build linkage, signatures, notarization, staples, Gatekeeper and quarantined copies. Deployment metadata is at most macOS 12; physical macOS 12 testing is separate.

The final signed image passed two parent-executed background sessions: unfinished privacy setup then normal OS Quit; cold file open, warm reopen to the same process, advancing video then normal OS Quit. Both returned naturally with no fallback or quarantine latch. The more extensive folder/Finish matrix was tested in the preceding development build, not repeated on the signed image.

Five full-trace Windows diagnostic quit routes exited naturally. They do not establish a fix for the intermittent release hang. Lower-overhead diagnostics have been compiled; independent execution hit a test-tool safety-rule rejection and the new native cache is not yet qualified. No security controls were disabled. Physical Windows comparison remains to be done.

## Other tracked issues

- Subtitle Duration stays disabled due to an upstream filter assertion.
- Mac background Command-W input was not successfully certified. Foreground shortcut tests are separate.
- The Mac launcher's media-library override remains to reconcile with the saved preference.
- Some startup QML component warnings remain.
- Complete dependency-source bundles, redistribution terms for design assets, and final matching installer/source sets are tracked before downloadable binary distribution. See [credits](CREDITS.md).

## Screenshots

The README now uses individual full-width macOS captures. The player was sized to900x600 points for readability and shows the maintainer's local copy of *What We Do in the Shadows* (2014). The settings capture is the actual redesigned dialog. Neither is an interface mockup; movie files are not included. Earlier Linux/generated-media screenshots remain in the gallery history.

Raw logs, local profiles, credentials and private machine state are not part of this public repository.
