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
