/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "dialogs/settings/settings_metadata.hpp"
#include <vlc/vlc.h>
#include <vlc_configuration.h>
#include <QTest>

class SettingsMetadataTest : public QObject
{
    Q_OBJECT
private slots:
    void installedOptions()
    {
        const char *args[] = {"--ignore-config", "--no-media-library", "--quiet"};
        auto *instance = libvlc_new(3, args);
        QVERIFY(instance);
        const auto options = settingsOptionMetadata();
        QVERIFY(!options.isEmpty());
        bool hasAccent = false;
        bool hasCaching = false;
        bool hasHotkey = false;
        for (const auto &option : options)
        {
            QVERIFY(!option.key.isEmpty());
            QVERIFY(!option.module.isEmpty());
            QVERIFY(config_FindConfig(qtu(option.key)));
            QVERIFY(settingsOptionMatches(option, option.key.toUpper()));
            QVERIFY(settingsOptionMatches(option, option.label));
            QVERIFY(!settingsOptionMatches(option, "this-option-does-not-exist-4.1"));
            hasAccent |= option.key == "qt-accent-color";
            hasCaching |= option.key == "network-caching";
            hasHotkey |= option.key == "key-play-pause";
        }
        QVERIFY(hasAccent);
        QVERIFY(hasCaching);
        QVERIFY(hasHotkey);
        const auto accent = config_FindConfig("qt-accent-color");
        QVERIFY(accent);
        QCOMPARE(QString::fromUtf8(accent->orig.psz), QString("default"));
        // The metadata bridge must not write or sample current values.
        QCOMPARE(config_GetInt("network-caching"), config_FindConfig("network-caching")->orig.i);
        libvlc_release(instance);
    }
};
QTEST_GUILESS_MAIN(SettingsMetadataTest)
#include "test_settings_metadata.moc"
