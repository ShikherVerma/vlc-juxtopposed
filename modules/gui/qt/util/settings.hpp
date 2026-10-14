/*****************************************************************************
 * settings.hpp: Qt preferences namespace
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *****************************************************************************/
#ifndef VLC_QT_SETTINGS_HPP
#define VLC_QT_SETTINGS_HPP

#include <QString>
#include <QFileInfo>
#include <QStringList>
#include <algorithm>
#include <vlc_configuration.h>
#include <cstdlib>

inline QString vlcQtSettingsOrganization()
{
#ifdef Q_OS_MACOS
    // Darwin already derives this directory from the main bundle identifier.
    // Native QSettings does not reliably honor HOME, so use the same namespace
    // for the main interface and legacy stream-output profile dialogs.
    char *directory = config_GetUserDir(VLC_CONFIG_DIR);
    const QString organization = directory
        ? QFileInfo(QString::fromUtf8(directory)).fileName()
        : QStringLiteral("org.videolan.vlc");
    free(directory);
    // QSettings expects a DNS organization and reverses its components for
    // CFPreferences. Pre-reverse the bundle ID so its native domain is retained.
    auto components = organization.split(QLatin1Char('.'));
    std::reverse(components.begin(), components.end());
    return components.join(QLatin1Char('.'));
#else
    return QStringLiteral("vlc");
#endif
}

#endif
