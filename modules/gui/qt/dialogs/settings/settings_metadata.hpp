/*****************************************************************************
 * settings_metadata.hpp : Read-only index of installed VLC option metadata
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifndef VLC_QT_SETTINGS_METADATA_HPP
#define VLC_QT_SETTINGS_METADATA_HPP

#include "qt.hpp"
#include <QVector>
#include <QString>

// Deliberately contains no values, passwords or inferred ranges/capabilities.
// Category migration is separate from VLC's native subcategory metadata.
struct SettingsOptionMetadata
{
    QString key;
    QString label;
    QString description;
    QString module;
    QString subcategory;
};

QVector<SettingsOptionMetadata> settingsOptionMetadata();
bool settingsOptionMatches(const SettingsOptionMetadata &, const QString &);

#endif
