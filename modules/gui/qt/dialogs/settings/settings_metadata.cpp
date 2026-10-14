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
    };
}
