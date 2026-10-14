/*****************************************************************************
 * settings_metadata.cpp : Read-only index of installed VLC option metadata
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings_metadata.hpp"
#include <vlc_configuration.h>
#include <vlc_config_cat.h>
#include <vlc_modules.h>
#include <algorithm>
#include <QObject>

QVector<SettingsOptionMetadata> settingsOptionMetadata()
{
    QVector<SettingsOptionMetadata> result;
    size_t count = 0;
    module_t **modules = module_list_get(&count);
    for (size_t i = 0; i < count; ++i)
    {
        unsigned size = 0;
        module_config_t *items = module_config_get(modules[i], &size);
        auto subcat = SUBCAT_UNKNOWN;
        for (size_t j = 0; j < size; ++j)
        {
            const auto &item = items[j];
            if (item.i_type == CONFIG_SUBCATEGORY)
            {
                subcat = static_cast<vlc_config_subcat>(item.value.i);
                continue;
            }
            // As in upstream Expert preferences: omit hidden/obsolete hints.
            // Keys are searchable here, but never edited by this shell.
            if (subcat == SUBCAT_UNKNOWN || subcat == SUBCAT_HIDDEN ||
                !CONFIG_ITEM(item.i_type) || !item.psz_name)
                continue;
            const char *category = vlc_config_subcat_GetName(subcat);
            result.push_back({QString::fromUtf8(item.psz_name),
                              qfut(item.psz_text ? item.psz_text : item.psz_name),
                              qfut(item.psz_longtext ? item.psz_longtext : ""),
                              QString::fromUtf8(module_get_object(modules[i])),
                              qfut(category ? category : "")});
        }
        module_config_free(items);
    }
    module_list_free(modules);
    const auto specs = settingsControlSpecs();
    for (auto &option : result)
        for (const auto &spec : specs)
            if (option.key == QString::fromLatin1(spec.key))
            {
                // Index the caption exactly as the page renders it (with units).
                const QString caption = *spec.units
                    ? qtr("%1 (%2)").arg(qfut(spec.label), qfut(spec.units)) : qfut(spec.label);
                if (option.label != caption)
                    option.label += QStringLiteral(" / ") + caption;
            }

    result.push_back({QStringLiteral("aout_DeviceSet"), qfut(N_("Output device (current session)")),
                      qfut(N_("Real devices from the current audio output; applies on Save, not persisted")),
                      QStringLiteral("player"), qfut(N_("Audio")), SettingsOptionMetadata::Kind::AudioOutputAPI});

    // Theme lives in QSettings rather than module config: index its real key
    // explicitly. Swatch aliases below retain their module-config backing.
    result.push_back({QStringLiteral("MainWindow/color-scheme-index"), qfut(N_("Theme")),
                      qfut(N_("Application colour scheme")), QStringLiteral("QSettings"), qfut(N_("General")),
                      SettingsOptionMetadata::Kind::QtSettings});
    static const char *const accents[] = {
        N_("Orange accent swatch"), N_("Red accent swatch"), N_("Yellow accent swatch"),
        N_("Green accent swatch"), N_("Cyan accent swatch"), N_("Blue accent swatch"),
        N_("Purple accent swatch"), N_("Pink accent swatch"), N_("Gray accent swatch")
    };
    for (int i = 0; i < 9; ++i)
        result.push_back({QStringLiteral("qt-accent-color"), qfut(accents[i]), qfut(N_("Accent colour")),
                          QStringLiteral("qt"), qfut(N_("General")), SettingsOptionMetadata::Kind::ModuleConfig, i});
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.key < b.key || (a.key == b.key && a.module < b.module);
    });
    return result;
}

bool settingsOptionMatches(const SettingsOptionMetadata &item, const QString &query)
{
    return item.key.contains(query, Qt::CaseInsensitive) ||
           item.label.contains(query, Qt::CaseInsensitive) ||
           item.description.contains(query, Qt::CaseInsensitive) ||
           item.module.contains(query, Qt::CaseInsensitive) ||
           item.subcategory.contains(query, Qt::CaseInsensitive);
}

QVector<SettingsControlSpec> settingsControlSpecs()
{
    return {
        {0, false, N_("Startup"), N_("Allow media interaction dialogs"), "interact", ""},
        {0, false, N_("Startup"), N_("Check for updates automatically"), "qt-updates-notif", ""},
        {0, false, N_("Diagnostics"), N_("Save a log"), "file-logging", ""},
        {0, false, N_("Diagnostics"), N_("Log file"), "logfile", ""},
        {0, false, N_("Diagnostics"), N_("Log details"), "log-verbose", ""},
        {0, true, N_("Advanced Options"), N_("Log format"), "logmode", ""},
        {0, true, N_("Advanced Options"), N_("Collect playback statistics"), "stats", ""},
        {0, true, N_("Advanced Options"), N_("Update check interval"), "qt-updates-days", N_("days")},
        {1, false, N_("Skin"), N_("Interface module"), "intf", ""},
        {1, false, N_("Skin"), N_("Skin file"), "skins2-last", ""},
        {1, false, N_("Window"), N_("Play video inside main window"), "embedded-video", ""},
        {1, false, N_("Window"), N_("Resize window to fit video"), "qt-video-autoresize", ""},
        {1, false, N_("Window"), N_("Keep video above other windows"), "video-on-top", ""},
        {1, false, N_("Window"), N_("Show native window decorations"), "qt-titlebar", ""},
        {1, false, N_("Window"), N_("Show the file name in the title bar"), "qt-name-in-title", ""},
        {1, false, N_("Window"), N_("Open fullscreen on"), "qt-fullscreen-screennumber", ""},
        {1, false, N_("Playback Controls"), N_("Start in minimal view"), "qt-minimal-view", ""},
        {1, false, N_("Playback Controls"), N_("Pin controls in windowed video"), "qt-pin-controls", ""},
        {1, false, N_("Playback Controls"), N_("Mouse / windowed controls hide timeout"), "mouse-hide-timeout", N_("ms")},
        {1, false, N_("Notifications and System Tray"), N_("Show a system tray icon"), "qt-system-tray", ""},
        {1, false, N_("Notifications and System Tray"), N_("Start minimized to the tray"), "qt-start-minimized", ""},
        {1, false, N_("Notifications and System Tray"), N_("Show a popup when the track changes"), "qt-notification", ""},
        {1, true, N_("Advanced Options"), N_("Window opacity"), "qt-opacity", ""},
        {1, true, N_("Advanced Options"), N_("Pause video when window is minimized"), "qt-pause-minimized", ""},
        {1, true, N_("Advanced Options"), N_("Bring VLC to the front when playback starts"), "qt-auto-raise", ""},
        {1, true, N_("Advanced Options"), N_("Show the file browser inside the open dialog"), "qt-embedded-open", ""},
        {1, true, N_("Advanced Options"), N_("Allow the app icon to change"), "qt-icon-change", ""},
        {1, true, N_("Advanced Options"), N_("Fullscreen controls opacity"), "qt-fs-opacity", ""},
        {4, false, N_("Startup"), N_("Continue where you left off"), "restore-playback-pos", ""},
        {4, false, N_("Startup"), N_("Start playing as soon as a file opens"), "playlist-autostart", ""},
        {4, false, N_("Startup"), N_("Default playback speed"), "rate", N_("factor")},
        {4, false, N_("Startup"), N_("Pause when another app uses audio"), "playlist-cork", ""},
        {4, false, N_("Tracks"), N_("Preferred video resolution"), "preferred-resolution", ""},
        {4, false, N_("Tracks"), N_("Preferred menu language"), "menu-language", ""},
        {4, false, N_("Tracks"), N_("Default audio track"), "audio-track", N_("index")},
        {4, false, N_("Tracks"), N_("Default subtitle track"), "sub-track", N_("index")},
        {4, false, N_("Order"), N_("Repeat current item"), "repeat", ""},
        {4, false, N_("Order"), N_("Repeat playlist"), "loop", ""},
        {4, false, N_("Order"), N_("Shuffle"), "random", ""},
        {4, false, N_("Order"), N_("Stop after each item"), "play-and-stop", ""},
        {4, false, N_("Order"), N_("Exit when playback finishes"), "play-and-exit", ""},
        {4, false, N_("Seeking"), N_("Fast seeking"), "input-fast-seek", ""},
        {4, false, N_("Files & Discs"), N_("Default DVD drive"), "dvd", ""},
        {4, false, N_("Files & Discs"), N_("Repair damaged AVI files"), "avi-index", ""},
        {4, false, N_("Files & Discs"), N_("Load matching .mkv files from the same folder"), "mkv-preload-local-dir", ""},
        {4, true, N_("Advanced Options"), N_("Time-shift buffer folder"), "input-timeshift-path", ""},
        {4, true, N_("Advanced Options"), N_("Time-shift buffer file size"), "input-timeshift-granularity", N_("bytes")},
        {6, false, N_("Output"), N_("Enable audio"), "audio", ""},
        {6, false, N_("Output"), N_("Audio output module"), "aout", ""},
        {6, false, N_("Output"), N_("Speaker setup"), "mix-mode", ""},
        {6, false, N_("Volume"), N_("Maximum volume"), "qt-max-volume", N_("%")},
        {6, false, N_("Volume"), N_("Remember last volume"), "volume-save", ""},
        {6, false, N_("Volume"), N_("Audio gain"), "gain", N_("factor")},
        {6, false, N_("Volume"), N_("Volume step"), "volume-step", N_("engine volume units")},
        {6, false, N_("Loudness"), N_("Audio filters"), "audio-filter", ""},
        {6, false, N_("Loudness"), N_("Normalize volume target level"), "norm-max-level", N_("factor")},
        {6, false, N_("Loudness"), N_("Replay gain"), "audio-replay-gain-mode", ""},
        {6, false, N_("Loudness"), N_("Replay gain preamp"), "audio-replay-gain-preamp", N_("dB")},
        {6, false, N_("Effects"), N_("Equalizer preset"), "equalizer-preset", ""},
        {6, false, N_("Effects"), N_("Equalizer bands"), "equalizer-bands", N_("dB, ten space-separated values")},
        {6, false, N_("Effects"), N_("Equalizer preamp"), "equalizer-preamp", N_("dB")},
        {6, false, N_("Effects"), N_("Two-pass equalizer"), "equalizer-2pass", ""},
        {6, false, N_("Effects"), N_("Pitch shift"), "pitch-shift", N_("semitones")},
        {6, false, N_("Tracks"), N_("Preferred audio language"), "audio-language", ""},
        {6, false, N_("Tracks"), N_("Audio delay"), "audio-desync", N_("ms")},
        {6, false, N_("Visualization"), N_("Visualizer"), "audio-visual", ""},
        {6, true, N_("Advanced Options"), N_("Time-stretch audio"), "audio-time-stretch", ""},
        {6, true, N_("Advanced Options"), N_("Detect Dolby Surround"), "force-dolby-surround", ""},
        {6, true, N_("Advanced Options"), N_("Audio resampler"), "audio-resampler", ""},
        {6, true, N_("Advanced Options"), N_("Media role"), "role", ""},
        {6, true, N_("Advanced Options"), N_("Replay gain default"), "audio-replay-gain-default", N_("dB")},
        {6, true, N_("Advanced Options"), N_("Replay gain peak protection"), "audio-replay-gain-peak-protection", ""},
        {7, false, N_("Tracks & Languages"), N_("Enable subtitles"), "spu", ""},
        {7, false, N_("Tracks & Languages"), N_("Preferred language"), "sub-language", ""},
        {7, false, N_("Tracks & Languages"), N_("Text encoding"), "subsdec-encoding", ""},
        {7, false, N_("Loading Subtitles"), N_("Load subtitle files automatically"), "sub-autodetect-file", ""},
        {7, false, N_("Loading Subtitles"), N_("File name matching"), "sub-autodetect-fuzzy", ""},
        {7, false, N_("Appearance"), N_("Font"), "freetype-font", ""},
        {7, false, N_("Appearance"), N_("Font size"), "sub-text-scale", N_("%")},
        {7, false, N_("Appearance"), N_("Bold"), "freetype-bold", ""},
        {7, false, N_("Appearance"), N_("Text color"), "freetype-color", N_("RGB")},
        {7, false, N_("Appearance"), N_("Text opacity"), "freetype-opacity", N_("0–255")},
        {7, false, N_("Appearance"), N_("Outline thickness"), "freetype-outline-thickness", N_("% of font size")},
        {7, false, N_("Appearance"), N_("Outline color"), "freetype-outline-color", N_("RGB")},
        {7, false, N_("Appearance"), N_("Shadow opacity"), "freetype-shadow-opacity", N_("0–255")},
        {7, false, N_("Appearance"), N_("Shadow color"), "freetype-shadow-color", N_("RGB")},
        {7, false, N_("Appearance"), N_("Shadow distance"), "freetype-shadow-distance", N_("factor of font size")},
        {7, false, N_("Appearance"), N_("Background color"), "freetype-background-color", N_("RGB")},
        {7, false, N_("Appearance"), N_("Background opacity"), "freetype-background-opacity", N_("0–255")},
        {7, false, N_("Appearance"), N_("Text alignment"), "subsdec-align", ""},
        {7, false, N_("Timing"), N_("Subtitle delay"), "sub-delay", N_("100 ms")},
        {7, false, N_("On-Screen Messages"), N_("Show feedback for shortcuts"), "osd", ""},
        {7, false, N_("On-Screen Messages"), N_("Show filename when a video starts"), "video-title-show", ""},
        {7, false, N_("On-Screen Messages"), N_("Show filename for"), "video-title-timeout", N_("ms")},
        {7, true, N_("Advanced Options"), N_("Load subtitles from these folders"), "sub-autodetect-path", ""},
        {7, true, N_("Advanced Options"), N_("Shadow angle"), "freetype-shadow-angle", N_("degrees")},
        {7, true, N_("Advanced Options"), N_("Text direction"), "freetype-text-direction", ""},
        {7, true, N_("Advanced Options"), N_("Monospace font"), "freetype-monofont", ""},
        {7, true, N_("Advanced Options"), N_("Outline opacity"), "freetype-outline-opacity", N_("0–255")},
        {7, true, N_("Advanced Options"), N_("Subtitle margin"), "sub-margin", N_("px")},
        {7, true, N_("Advanced Options"), N_("Subtitle frame rate"), "sub-fps", N_("fps")},
        {7, true, N_("Advanced Options"), N_("Detect UTF-8 automatically"), "subsdec-autodetect-utf8", ""},
        {7, true, N_("Advanced Options"), N_("Text renderer"), "text-renderer", ""},
    };
}
