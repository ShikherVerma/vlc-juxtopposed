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
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include "../../../../lib/libvlc_internal.h"
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTest>
#include <QSet>
#include <algorithm>

class SettingsMetadataTest : public QObject
{
    Q_OBJECT
private:
    static QVariant read(const char *key, int type)
    {
        if (IsConfigStringType(type))
        {
            char *text = config_GetPsz(key);
            const QString value = QString::fromUtf8(text ? text : "");
            free(text);
            return value;
        }
        if (type == CONFIG_ITEM_FLOAT) return config_GetFloat(key);
        return QVariant::fromValue<qlonglong>(config_GetInt(key));
    }
    static void put(const char *key, int type, const QVariant &value)
    {
        if (IsConfigStringType(type)) config_PutPsz(key, qtu(value.toString()));
        else if (type == CONFIG_ITEM_FLOAT) config_PutFloat(key, value.toFloat());
        else config_PutInt(key, value.toLongLong());
    }
private slots:
    void categoryConfigRoundTrip()
    {
        QTemporaryDir scratch;
        QVERIFY(scratch.isValid());
        const QString output = qEnvironmentVariable("VLC_SETTINGS_EVIDENCE", scratch.path());
        QVERIFY(QDir().mkpath(output));
        const QByteArray configArg = ("--config=" + output + "/category-roundtrip.vlcrc").toUtf8();
        const char *args[] = {configArg.constData(), "--no-ignore-config", "--no-media-library", "--quiet"};
        auto *instance = libvlc_new(4, args);
        QVERIFY(instance);
        struct Value { QString key; int type; QVariant changed; QVariant defaults; };
        QVector<Value> values;
        QJsonArray evidence;
        QJsonArray searchIndex;
        for (const auto &option : settingsOptionMetadata())
            searchIndex.push_back(QJsonObject{{"key", option.key}, {"label", option.label},
                {"description", option.description}, {"module", option.module}, {"subcategory", option.subcategory}});
        for (const auto &spec : settingsControlSpecs())
        {
            const auto *metadata = config_FindConfig(spec.key);
            if (!metadata) continue;
            const int type = metadata->i_type;
            QVariant defaults, changed;
            if (IsConfigStringType(type))
            {
                defaults = QString::fromUtf8(metadata->orig.psz ? metadata->orig.psz : "");
                changed = metadata->list_count > 1
                    ? QString::fromUtf8(metadata->list.psz[1]) : QStringLiteral("lane46-config-roundtrip");
                if (QString::fromLatin1(spec.key) == "logfile") changed = output + "/test-log.txt";
                if (QString::fromLatin1(spec.key) == "intf") changed = QStringLiteral("qt");
                if (type == CONFIG_ITEM_MODULE || type == CONFIG_ITEM_MODULE_LIST)
                {
                    size_t count = 0;
                    module_t **modules = module_list_get(&count);
                    for (size_t i = 0; i < count; ++i)
                        if (module_provides(modules[i], metadata->psz_type))
                        {
                            changed = QString::fromUtf8(module_get_object(modules[i]));
                            break;
                        }
                    module_list_free(modules);
                }
            }
            else if (type == CONFIG_ITEM_FLOAT)
            {
                defaults = metadata->orig.f;
                changed = std::clamp(.5f, metadata->min.f, metadata->max.f);
            }
            else
            {
                defaults = QVariant::fromValue<qlonglong>(metadata->orig.i);
                changed = QVariant::fromValue<qlonglong>(type == CONFIG_ITEM_BOOL ? !metadata->orig.i
                    : metadata->list_count > 1 ? metadata->list.i[metadata->list_count-1]
                    : metadata->orig.i == metadata->min.i ? metadata->orig.i + 1 : metadata->min.i);
                // Use a practical but real unconstrained integer, not a mock bound.
                if (type == CONFIG_ITEM_INTEGER && metadata->list_count == 0 && metadata->min.i == INT64_MIN) changed = QVariant::fromValue<qlonglong>(100);
            }
            if (type == CONFIG_ITEM_RGB) changed = QVariant::fromValue<qlonglong>(0x19a234);
            if (IsConfigStringType(type) && !metadata->list_count && type != CONFIG_ITEM_MODULE && type != CONFIG_ITEM_MODULE_LIST)
            {
                if (QString::fromLatin1(spec.key).endsWith("language")) changed = QStringLiteral("fr");
                if (QString::fromLatin1(spec.key).startsWith("freetype-")) changed = QStringLiteral("DejaVu Sans");
                if (QString::fromLatin1(spec.key) == "equalizer-bands") changed = QStringLiteral("3 0 0 0 0 0 0 0 0 0");
            }
            put(spec.key, type, changed);
            QCOMPARE(read(spec.key, type), changed);
            values.push_back({QString::fromLatin1(spec.key), type, changed, defaults});
            QJsonObject entry{{"key", QString::fromLatin1(spec.key)}, {"label", qfut(spec.label)},
                {"type", type}, {"category", spec.category}, {"advanced", spec.advanced},
                {"units", qfut(spec.units)}, {"hasChoices", metadata->list_count > 0 || type == CONFIG_ITEM_MODULE},
                {"changed", QJsonValue::fromVariant(changed)}, {"default", QJsonValue::fromVariant(defaults)}};
            if (type == CONFIG_ITEM_INTEGER || type == CONFIG_ITEM_RGB)
            {
                entry["min"] = QString::number(metadata->min.i);
                entry["max"] = QString::number(metadata->max.i);
            }
            if (type == CONFIG_ITEM_FLOAT)
            {
                entry["min"] = metadata->min.f;
                entry["max"] = metadata->max.f;
            }
            QJsonArray choices;
            for (unsigned i = 0; i < metadata->list_count; ++i)
                choices.push_back(QJsonObject{{"label", qfut(metadata->list_text[i])}, {"value", IsConfigStringType(type)
                    ? QJsonValue(QString::fromUtf8(metadata->list.psz[i])) : QJsonValue(metadata->list.i[i])}});
            if (type == CONFIG_ITEM_MODULE)
            {
                choices.push_back(QJsonObject{{"label", "Automatic"}, {"value", "any"}});
                if (QString::fromLatin1(spec.key) == "audio-visual")
                    choices.push_back(QJsonObject{{"label", "Disabled"}, {"value", "none"}});
                size_t count = 0;
                module_t **modules = module_list_get(&count);
                for (size_t i = 0; i < count; ++i)
                    if (module_provides(modules[i], metadata->psz_type))
                        choices.push_back(QJsonObject{{"label", qfut(module_GetLongName(modules[i]))},
                            {"value", QString::fromUtf8(module_get_object(modules[i]))}});
                module_list_free(modules);
            }
            entry["choices"] = choices;
            evidence.push_back(entry);
        }
        config_PutInt("network-caching", 1234);
        QCOMPARE((config_SaveConfigFile)(instance->p_libvlc_int), 0);
        QFile saved(output + "/category-roundtrip.vlcrc");
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto bytes = saved.readAll();
        QVERIFY(bytes.contains("network-caching=1234"));
        saved.close();
        libvlc_release(instance);
        instance = libvlc_new(4, args);
        QVERIFY(instance);
        for (const auto &value : values)
            QCOMPARE(read(qtu(value.key), value.type), value.changed);
        QCOMPARE(config_GetInt("network-caching"), int64_t(1234));
        for (const auto &value : values) put(qtu(value.key), value.type, value.defaults);
        QCOMPARE((config_SaveConfigFile)(instance->p_libvlc_int), 0);
        libvlc_release(instance);
        instance = libvlc_new(4, args);
        QVERIFY(instance);
        for (const auto &value : values)
            QCOMPARE(read(qtu(value.key), value.type), value.defaults);
        QCOMPARE(config_GetInt("network-caching"), int64_t(1234));
        QFile json(output + "/category-roundtrip.json");
        QVERIFY(json.open(QIODevice::WriteOnly));
        QVERIFY(json.write(QJsonDocument(QJsonObject{{"readSaveRestartReadbackResetPassed", true}, {"controls", evidence}, {"searchIndex", searchIndex}}).toJson()) > 0);
        libvlc_release(instance);
    }
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
            if (option.kind == SettingsOptionMetadata::Kind::ModuleConfig)
                QVERIFY(config_FindConfig(qtu(option.key)));
            else if (option.kind == SettingsOptionMetadata::Kind::AudioOutputAPI)
            {
                QCOMPARE(option.key, QStringLiteral("aout_DeviceSet"));
                QVERIFY(!config_FindConfig(qtu(option.key)));
            }
            else
            {
                QCOMPARE(option.kind, SettingsOptionMetadata::Kind::QtSettings);
                QCOMPARE(option.key, QStringLiteral("MainWindow/color-scheme-index"));
                QCOMPARE(option.label, QStringLiteral("Theme"));
                QVERIFY(!config_FindConfig(qtu(option.key)));
                QTemporaryDir scratch;
                QVERIFY(scratch.isValid());
                const auto file = scratch.filePath("vlc-qt-interface.conf");
                for (int index : {0, 1, 2})
                {
                    {
                        QSettings settings(file, QSettings::IniFormat);
                        settings.setValue(option.key, index);
                        settings.sync();
                        QCOMPARE(settings.status(), QSettings::NoError);
                    }
                    QSettings reloaded(file, QSettings::IniFormat);
                    QCOMPARE(reloaded.value(option.key).toInt(), index);
                }
            }
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
        bool hasThemeLabel = false;
        bool hasThemeKey = false;
        QSet<QString> swatchLabels;
        QSet<int> swatchDestinations;
        for (const auto &option : options)
        {
            hasThemeLabel |= settingsOptionMatches(option, QStringLiteral("Theme"));
            hasThemeKey |= settingsOptionMatches(option, QStringLiteral("MainWindow/color-scheme-index"));
            if (option.key == QStringLiteral("qt-accent-color") && option.label.endsWith(QStringLiteral("accent swatch")))
            {
                QCOMPARE(option.kind, SettingsOptionMetadata::Kind::ModuleConfig);
                QVERIFY(option.accentSwatch >= 0 && option.accentSwatch < 9);
                swatchLabels.insert(option.label);
                swatchDestinations.insert(option.accentSwatch);
            }
        }
        QVERIFY(hasThemeLabel);
        QVERIFY(hasThemeKey);
        QCOMPARE(swatchLabels.size(), 9);
        QCOMPARE(swatchDestinations.size(), 9);
        for (const auto &label : swatchLabels)
            QVERIFY(std::any_of(options.cbegin(), options.cend(), [&label](const SettingsOptionMetadata &option) {
                return option.label == label && settingsOptionMatches(option, label);
            }));
        const auto accent = config_FindConfig("qt-accent-color");
        QVERIFY(accent);
        QCOMPARE(QString::fromUtf8(accent->orig.psz), QString("default"));
        const auto specs = settingsControlSpecs();
        QSet<QString> keys;
        QSet<int> presentCategories;
        int present = 0;
        for (const auto &spec : specs)
        {
            QVERIFY(spec.category == 0 || spec.category == 1 || spec.category == 4 || spec.category == 6 || spec.category == 7);
            const QString key = QString::fromLatin1(spec.key);
            QVERIFY(!keys.contains(key));
            keys.insert(key);
            const auto *metadata = config_FindConfig(spec.key);
            if (!metadata) continue; // unavailable modules / UPDATE_CHECK
            ++present;
            presentCategories.insert(spec.category);
            // Search must match the caption exactly as rendered, units included.
            const QString caption = *spec.units
                ? qtr("%1 (%2)").arg(qfut(spec.label), qfut(spec.units)) : qfut(spec.label);
            QVERIFY2(std::any_of(options.cbegin(), options.cend(), [&](const SettingsOptionMetadata &option) {
                return option.key == key && settingsOptionMatches(option, caption);
            }), qtu(caption));
            QVERIFY(metadata->i_type == CONFIG_ITEM_BOOL ||
                    metadata->i_type == CONFIG_ITEM_INTEGER || metadata->i_type == CONFIG_ITEM_RGB ||
                    metadata->i_type == CONFIG_ITEM_FLOAT || IsConfigStringType(metadata->i_type));
            if (metadata->i_type == CONFIG_ITEM_FLOAT)
            {
                QVERIFY(metadata->min.f <= metadata->orig.f);
                QVERIFY(metadata->orig.f <= metadata->max.f);
            }
            if (metadata->i_type == CONFIG_ITEM_INTEGER || metadata->i_type == CONFIG_ITEM_RGB)
            {
                QVERIFY(metadata->min.i <= metadata->orig.i);
                QVERIFY(metadata->orig.i <= metadata->max.i);
            }
        }
        QVERIFY(present >= 23); // optional module controls may be unavailable
        for (int category : {0, 1, 4, 6, 7}) QVERIFY(presentCategories.contains(category));
        QCOMPARE(config_FindConfig("qt-opacity")->min.f, .1f);
        QCOMPARE(config_FindConfig("qt-opacity")->max.f, 1.f);
        QCOMPARE(config_FindConfig("qt-fs-sensitivity")->max.i, int64_t(4000));
        QCOMPARE(config_FindConfig("mouse-hide-timeout")->min.i, INT64_MIN);
        // The metadata bridge must not write or sample current values.
        QCOMPARE(config_GetInt("network-caching"), config_FindConfig("network-caching")->orig.i);
        libvlc_release(instance);
    }
};
QTEST_GUILESS_MAIN(SettingsMetadataTest)
#include "test_settings_metadata.moc"
