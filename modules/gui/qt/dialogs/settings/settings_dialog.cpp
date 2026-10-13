/*****************************************************************************
 * settings_dialog.cpp : Authored settings shell (category panels pending)
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "settings_dialog.hpp"
#include "maininterface/mainctx.hpp"
#include "util/color_scheme_model.hpp"
#include "style/vlcicons.hpp"
#include "style/systempalette.hpp"
#include <vlc_configuration.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QScreen>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace {
const char *const categoryNames[] = {
    N_("General"), N_("Interface"), N_("Privacy"), N_("Controls"),
    N_("Playback"), N_("Video"), N_("Audio"), N_("Subtitles"), N_("Library"),
    N_("Network"), N_("Performance"), N_("Recording"), N_("Extensions")
};
const char *const categoryTitles[] = {
    N_("Global Settings"), N_("Interface Settings"), N_("Privacy Settings"),
    N_("Mouse, Keyboard & Remote Controls"), N_("Playback Preferences"),
    N_("Video Defaults"), N_("Audio Defaults"), N_("Subtitle Defaults"),
    N_("Playlists, Folders & Files"), N_("Network Connections & Security"),
    N_("Performance Settings"), N_("Recording, Casting & Streaming Out"),
    N_("Add-ons & Scripts")
};
const QChar categoryIcons[] = {
    VLCIcons::preferences, VLCIcons::interface, VLCIcons::privacy, VLCIcons::hotkeys,
    VLCIcons::playback, VLCIcons::video, VLCIcons::audio, VLCIcons::audiosub,
    VLCIcons::folder, VLCIcons::topbar_network, VLCIcons::preferences,
    VLCIcons::record, VLCIcons::plugins
};
// Swatches from native 999:5047. Orange means the real module default,
// not a new hardcoded configuration default.
const char *const swatchColors[] = {
    "#d37918", "#e15032", "#c49e23", "#19a234", "#18aad3",
    "#185dd3", "#8e42ff", "#f052b3", "#afafaf"
};
QColor selectedAccentWell(const QColor &color, bool light)
{
    // Same authored HSV-family relationship as SystemPalette::updateAccent.
    const QColor main(light ? "#e8851a" : "#d37918");
    const QColor authored(light ? "#d8ba9a" : "#371f06");
    qreal hue = color.hsvHueF();
    if (hue < 0)
        hue = 0;
    hue += authored.hsvHueF() - main.hsvHueF();
    hue -= std::floor(hue);
    const qreal saturation = std::clamp<qreal>(color.hsvSaturationF() * authored.hsvSaturationF() / main.hsvSaturationF(), 0, 1);
    const qreal value = std::clamp<qreal>(color.valueF() * authored.valueF() / main.valueF(), 0, 1);
    return QColor::fromHsvF(hue, saturation, value);
}
QString configAccent()
{
    char *value = config_GetPsz("qt-accent-color");
    const QString result = QString::fromUtf8(value ? value : "");
    free(value);
    return result;
}
QIcon glyphIcon(QChar glyph, const QColor &color)
{
    QPixmap image(24, 24);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QFont font(QStringLiteral("VLCIcons"));
    font.setPixelSize(24);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(image.rect(), Qt::AlignCenter, QString(glyph));
    return QIcon(image);
}
QLabel *label(const QString &text, QWidget *parent = nullptr)
{
    auto *result = new QLabel(text, parent);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    return result;
}
}

SettingsDialog::SettingsDialog(QWindow *parent, qt_intf_t *intf)
    : QVLCDialog(parent, intf), m_metadata(settingsOptionMetadata())
{
    setObjectName("settingsShell");
    setWindowTitle(qtr("Settings"));
    setWindowRole("vlc-settings");
    setWindowModality(Qt::WindowModal);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(760, 480);
    m_savedAccent = m_accent = configAccent();
    // Read the actual add_string default; reset never touches other keys.
    module_config_t *accent = config_FindConfig("qt-accent-color");
    if (accent && accent->orig.psz)
        m_defaultAccent = QString::fromUtf8(accent->orig.psz);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *surface = new QWidget;
    surface->setObjectName("settingsSurface");
    outer->addWidget(surface);
    auto *layout = new QVBoxLayout(surface);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName("settingsHeader");
    header->setFixedHeight(50);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 8, 16, 8);
    headerLayout->setSpacing(32);
    m_search = new QLineEdit;
    m_search->setObjectName("settingsSearch");
    m_search->setPlaceholderText(qtr("Search"));
    m_search->setAccessibleName(qtr("Search settings labels and configuration keys"));
    m_search->setClearButtonEnabled(true);
    // QLineEdit's action icon is forced to the platform's small (16px)
    // metric. Use the authored 24px glyph and explicit source text inset.
    m_search->setTextMargins(32, 0, 0, 0);
    auto *searchIcon = new QLabel(QString(VLCIcons::search), m_search);
    searchIcon->setGeometry(8, 5, 24, 24);
    searchIcon->setAlignment(Qt::AlignCenter);
    searchIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    searchIcon->setStyleSheet(QStringLiteral("font-family:VLCIcons; font-size:24px; color:#6b6b6b; background:transparent;"));
    m_search->setFixedSize(200, 34);
    headerLayout->addWidget(m_search);
    m_title = label(qtr("Global Settings"));
    m_title->setObjectName("settingsTitle");
    headerLayout->addWidget(m_title, 1);
    auto *close = new QPushButton;
    close->setObjectName("settingsClose");
    close->setAccessibleName(qtr("Close settings without saving"));
    close->setFixedSize(28, 28);
    close->setIconSize(QSize(24, 24));
    close->setIcon(glyphIcon(VLCIcons::close, QColor("#8f8f8f")));
    headerLayout->addWidget(close);
    layout->addWidget(header);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(8, 8, 24, 8);
    body->setSpacing(32);
    auto *navigation = new QWidget;
    navigation->setObjectName("settingsNavigation");
    auto *navigationScroll = new QScrollArea;
    navigationScroll->setObjectName("settingsNavigationScroll");
    navigationScroll->setFixedWidth(200);
    navigationScroll->setFrameShape(QFrame::NoFrame);
    navigationScroll->setWidgetResizable(true);
    navigationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigationScroll->setWidget(navigation);
    navigationScroll->viewport()->setAutoFillBackground(false);
    navigation->setAutoFillBackground(false);
    auto *nav = new QVBoxLayout(navigation);
    nav->setContentsMargins(6, 6, 6, 6);
    nav->setSpacing(8);
    for (int i = 0; i < 13; ++i)
    {
        if (i == 0 || i == 4 || i == 9)
        {
            auto *group = label(i == 0 ? qtr("App") : i == 4 ? qtr("Media") : qtr("System"));
            group->setObjectName("settingsGroup");
            group->setFixedHeight(33);
            group->setContentsMargins(10, 0, 0, 0);
            nav->addWidget(group);
        }
        // Two Inter spaces preserve the authored icon-to-label gap in the
        // native QPushButton layout; accessibility exposes the plain label.
        auto *button = new QPushButton(QStringLiteral("  ") + qfut(categoryNames[i]));
        button->setAccessibleName(qfut(categoryNames[i]));
        button->setObjectName(QStringLiteral("settingsCategory%1").arg(i));
        button->setProperty("category", true);
        button->setCheckable(true);
        button->setFixedHeight(40);
        button->setIconSize(QSize(24, 24));
        m_categories[i] = button;
        nav->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            m_search->clear();
            selectCategory(i);
        });
    }
    nav->addStretch();
    auto *all = new QPushButton(qtr("All settings"));
    all->setObjectName("settingsAll");
    all->setToolTip(qtr("Open the original VLC preferences. Simple, All and Expert views remain available."));
    nav->addWidget(all);
    connect(all, &QPushButton::clicked, this, &SettingsDialog::openAllSettings);
    body->addWidget(navigationScroll);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(0, 12, 0, 8);
    content->setSpacing(16);
    m_pages = new QStackedWidget;
    auto *categoryPage = new QWidget;
    auto *panel = new QVBoxLayout(categoryPage);
    panel->setContentsMargins(0, 0, 0, 0);
    panel->setSpacing(24);
    m_accentRow = new QWidget;
    auto *accentLayout = new QVBoxLayout(m_accentRow);
    accentLayout->setContentsMargins(0, 0, 0, 0);
    accentLayout->setSpacing(0);
    auto *appearance = label(qtr("Appearance"));
    appearance->setObjectName("settingsSection");
    appearance->setFixedHeight(39);
    accentLayout->addWidget(appearance);
    auto *appearancePending = label(qtr("Language and theme controls are pending the General panel.\nThe shell follows VLC’s current theme."));
    appearancePending->setFixedHeight(92);
    accentLayout->addWidget(appearancePending);
    auto *row = new QHBoxLayout;
    row->setSpacing(16);
    auto *accentLabel = label(qtr("Accent color"));
    accentLabel->setToolTip(qtr("qt-accent-color — applies after restarting VLC"));
    row->addWidget(accentLabel, 1);
    auto *swatches = new QWidget;
    swatches->setObjectName("settingsSwatches");
    swatches->setFixedSize(320, 36);
    for (int i = 0; i < 9; ++i)
    {
        auto *button = new QPushButton;
        button->setObjectName(QStringLiteral("settingsAccent%1").arg(i));
        const int left = 2 + (i * 320 + 4) / 9;
        button->setParent(swatches);
        button->setGeometry(left, 2, 32, 32);
        button->setCheckable(true);
        const QString value = i == 0 ? m_defaultAccent : QString::fromLatin1(swatchColors[i]);
        button->setAccessibleName(i == 0 ? qtr("Default orange accent") : qtr("Accent %1").arg(value));
        button->setToolTip(i == 0 ? qtr("Module default: %1").arg(value) : value);
        m_swatches[i] = button;
        connect(button, &QPushButton::clicked, this, [this, value] {
            m_accent = value;
            updateAccent();
        });
    }
    row->addWidget(swatches);
    row->setContentsMargins(0, 5, 0, 5);
    accentLayout->addLayout(row);
    m_accentValue = label(QString());
    accentLayout->addWidget(m_accentValue);
    m_accentRow->setEnabled(accent && !m_defaultAccent.isEmpty());
    panel->addWidget(m_accentRow);
    m_pending = label(QString());
    m_pending->setObjectName("settingsPending");
    panel->addWidget(m_pending);
    m_advancedButton = new QPushButton;
    m_advancedButton->setObjectName("settingsAdvanced");
    m_advancedButton->setFixedHeight(38);
    panel->addWidget(m_advancedButton);
    panel->addStretch();
    connect(m_advancedButton, &QPushButton::clicked, this, [this] {
        m_advanced[m_category] = !m_advanced[m_category];
        selectCategory(m_category);
    });
    m_pages->addWidget(categoryPage);

    auto *searchPage = new QWidget;
    auto *searchLayout = new QVBoxLayout(searchPage);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    m_resultCount = label(QString());
    searchLayout->addWidget(m_resultCount);
    m_results = new QTreeWidget;
    m_results->setObjectName("settingsResults");
    m_results->setHeaderLabels({qtr("Setting / configuration key"), qtr("Module / VLC category")});
    m_results->setRootIsDecorated(false);
    m_results->setColumnWidth(0, 475);
    m_results->setWordWrap(true);
    m_results->setAlternatingRowColors(false);
    searchLayout->addWidget(m_results, 1);
    auto *searchHelp = label(qtr("Read-only metadata from installed VLC modules. Category panels are pending; use All settings to edit these options."));
    searchLayout->addWidget(searchHelp);
    auto *legacy = new QPushButton(qtr("Open All settings"));
    searchLayout->addWidget(legacy);
    connect(legacy, &QPushButton::clicked, this, &SettingsDialog::openAllSettings);
    m_pages->addWidget(searchPage);
    m_search->installEventFilter(this);
    m_results->installEventFilter(this);
    auto *pageScroll = new QScrollArea;
    pageScroll->setObjectName("settingsPageScroll");
    pageScroll->setFrameShape(QFrame::NoFrame);
    pageScroll->setWidgetResizable(true);
    pageScroll->setWidget(m_pages);
    pageScroll->viewport()->setAutoFillBackground(false);
    m_pages->setAutoFillBackground(false);
    content->addWidget(pageScroll, 1);
    m_restart = label(QString());
    m_restart->setObjectName("settingsRestart");
    content->addWidget(m_restart);
    auto *actions = new QHBoxLayout;
    auto *reset = new QPushButton(qtr("Reset accent"));
    reset->setObjectName("settingsReset");
    reset->setEnabled(m_accentRow->isEnabled());
    actions->addWidget(reset);
    actions->addStretch();
    auto *cancel = new QPushButton(qtr("Cancel"));
    cancel->setObjectName("settingsCancel");
    actions->addWidget(cancel);
    auto *save = new QPushButton(qtr("Save"));
    save->setObjectName("settingsSave");
    actions->addWidget(save);
    content->addLayout(actions);
    body->addLayout(content, 1);
    layout->addLayout(body, 1);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, [this] { if (saveAccent()) accept(); });
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetAccent);
    connect(m_search, &QLineEdit::textChanged, this, &SettingsDialog::search);
    auto *find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, m_search, [this] { m_search->setFocus(); m_search->selectAll(); });
    // Reuse VLC's actual theme/provider detection, including System fallback
    // and OS-theme notifications; do not infer a separate preferences theme.
    m_palette = new SystemPalette(this);
    m_palette->setCtx(p_intf->p_mi);
    m_palette->setSource(p_intf->p_mi ? p_intf->p_mi->getColorScheme()->currentScheme() : ColorSchemeModel::Night);
    connect(m_palette, &SystemPalette::paletteChanged, this, &SettingsDialog::applyTheme);
    if (p_intf->p_mi)
        connect(p_intf->p_mi->getColorScheme(), &ColorSchemeModel::currentChanged, this, [this] {
            m_palette->setSource(p_intf->p_mi->getColorScheme()->currentScheme());
        });
    applyTheme();
    selectCategory(0);
    updateAccent();
    // Keep the authored size on large desktops, but never place the footer
    // outside the usable screen (including the Windows taskbar).
    const QRect available = screen()->availableGeometry();
    resize(QSize(1048, 865).boundedTo(available.size()));
    move(available.topLeft() + QPoint((available.width() - width()) / 2,
                                     (available.height() - height()) / 2));
}

void SettingsDialog::selectCategory(int category)
{
    m_category = category;
    m_pages->setCurrentIndex(0);
    const bool light = !m_palette->property("isDark").toBool();
    const QColor active = QColor(m_accent).isValid() ? QColor(m_accent) : QColor(light ? "#e8851a" : swatchColors[0]);
    for (int i = 0; i < 13; ++i)
    {
        m_categories[i]->setChecked(i == category);
        m_categories[i]->setIcon(glyphIcon(categoryIcons[i], i == category ? active : QColor(light ? "#2e2e2e" : "#8f8f8f")));
    }
    m_title->setText(qfut(categoryTitles[category]));
    m_accentRow->setVisible(category == 0);
    m_pending->setText(qtr("%1 — %2 panel pending.\nNo category controls have been migrated yet. All existing options remain available in All settings.")
        .arg(qfut(categoryNames[category]), m_advanced[category] ? qtr("Advanced") : qtr("Simple")));
    m_advancedButton->setText(m_advanced[category] ? qtr("Simple Options  ‹") : qtr("Advanced Options  ›"));
}

void SettingsDialog::search(const QString &text)
{
    const QString query = text.trimmed();
    if (query.isEmpty())
    {
        selectCategory(m_category);
        return;
    }
    m_pages->setCurrentIndex(1);
    m_title->setText(qtr("Search Settings"));
    m_results->clear();
    int options = 0;
    int categories = 0;
    for (int i = 0; i < 13; ++i)
    {
        const QString group = i < 4 ? qtr("App") : i < 9 ? qtr("Media") : qtr("System");
        if (!qfut(categoryNames[i]).contains(query, Qt::CaseInsensitive) &&
            !qfut(categoryTitles[i]).contains(query, Qt::CaseInsensitive) &&
            !group.contains(query, Qt::CaseInsensitive))
            continue;
        auto *item = new QTreeWidgetItem(m_results, {qtr("%1 — category panel pending").arg(qfut(categoryNames[i])), qtr("Category")});
        item->setData(0, Qt::UserRole, i);
        ++categories;
    }
    for (const auto &option : m_metadata)
    {
        if (!settingsOptionMatches(option, query))
            continue;
        auto *item = new QTreeWidgetItem(m_results, {option.label + "\n" + option.key, option.module + "\n" + option.subcategory});
        item->setToolTip(0, option.description);
        ++options;
    }
    m_resultCount->setText(qtr("%1 options · %2 categories — labels and real configuration keys").arg(options).arg(categories));
    // Categories are navigation results; options remain read-only until mapped.
    if (!m_results->property("navigationConnected").toBool())
    {
        connect(m_results, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
            const QVariant category = item->data(0, Qt::UserRole);
            if (category.isValid())
            {
                m_search->clear();
                selectCategory(category.toInt());
            }
        });
        m_results->setProperty("navigationConnected", true);
    }
}

void SettingsDialog::updateAccent()
{
    for (int i = 0; i < 9; ++i)
        m_swatches[i]->setChecked(m_accent.compare(i == 0 ? m_defaultAccent : QString::fromLatin1(swatchColors[i]), Qt::CaseInsensitive) == 0);
    m_accentValue->setText(qtr("qt-accent-color: %1 · Restart needed after saving").arg(m_accent));
    m_restart->setText(m_accent == m_savedAccent ? qtr("Accent changes require restarting VLC.") : qtr("Unsaved accent change — restart VLC after saving."));
}

bool SettingsDialog::saveAccent()
{
    if (m_accent == m_savedAccent)
        return true;
    const QString previous = configAccent();
    config_PutPsz("qt-accent-color", qtu(m_accent));
    if (config_SaveConfigFile(p_intf) != 0)
    {
        config_PutPsz("qt-accent-color", qtu(previous));
        QMessageBox::warning(this, qtr("Cannot save Configuration"), qtr("Preferences file could not be saved. The accent change was not applied."));
        return false;
    }
    m_savedAccent = m_accent;
    updateAccent();
    return true;
}

void SettingsDialog::resetAccent()
{
    if (QMessageBox::question(this, qtr("Reset accent"),
        qtr("Reset only qt-accent-color to its module default and save it now? Other VLC preferences will not be reset. This cannot be undone by Cancel."),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok)
        return;
    m_accent = m_defaultAccent;
    saveAccent();
    updateAccent();
}

void SettingsDialog::openAllSettings()
{
    if (m_accent != m_savedAccent)
    {
        const auto response = QMessageBox::question(this, qtr("Unsaved accent change"),
            qtr("Save the accent before opening the original VLC preferences?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (response == QMessageBox::Cancel || (response == QMessageBox::Save && !saveAccent()))
            return;
    }
    emit allSettingsRequested();
}

void SettingsDialog::applyTheme()
{
    const bool light = !m_palette->property("isDark").toBool();
    const QString canvas = light ? "#e7e7e7" : "#0e0e0e";
    const QString well = light ? "#dedede" : "#090909";
    const QString panel = light ? "#d5d5d5" : "#181818";
    const QString fg = light ? "#2e2e2e" : "#8f8f8f";
    const QString primary = light ? "#161616" : "#c6c6c6";
    const QString selected = light ? "#dddddd" : "#222222";
    const QString focus = m_palette->get_accentMain().name();
    setStyleSheet(QStringLiteral(
        "QWidget { color:%1; font-family:Inter; font-size:14px; }"
        "QWidget#settingsSurface { background:%2; border-radius:14px; }"
        "QWidget#settingsHeader { background:%3; border-top-left-radius:14px; border-top-right-radius:14px; }"
        "QWidget#settingsNavigation { background:%4; border-radius:6px; }"
        "QWidget#settingsSwatches { background:%3; border-radius:6px; }"
        "QLabel { background:transparent; }"
        "QLabel#settingsTitle, QLabel#settingsSection { font-size:16px; }"
        "QLabel#settingsSection { color:%5; }"
        "QPushButton { background:%4; border:1px solid transparent; border-radius:4px; padding:8px 12px; }"
        "QPushButton:hover, QPushButton:checked { background:%6; color:%5; }"
        "QPushButton:focus { border:1px solid %7; }"
        "QPushButton[category=true] { text-align:left; padding:0px 8px; }"
        "QPushButton#settingsClose { background:transparent; padding:0; }"
        "QLineEdit { background:%4; border:1px solid transparent; border-radius:6px; padding:6px 10px; }"
        "QLineEdit:focus { border:1px solid %7; }"
        "QTreeWidget { background:%2; border:0; }"
        "QTreeWidget::item { padding:8px 4px; }"
        "QTreeWidget::item:selected { background:%6; color:%5; }"
        "QHeaderView::section { background:%4; color:%1; border:0; padding:8px; }"
        "QLabel#settingsGroup { color:#6b6b6b; }"
    ).arg(fg, canvas, well, panel, primary, selected, focus));
    const QColor active = QColor(m_accent).isValid() ? QColor(m_accent) : QColor(light ? "#e8851a" : swatchColors[0]);
    for (int i = 0; i < 13; ++i)
        m_categories[i]->setIcon(glyphIcon(categoryIcons[i], m_categories[i]->isChecked() ? active : QColor(fg)));
    findChild<QPushButton *>(QStringLiteral("settingsClose"))->setIcon(glyphIcon(VLCIcons::close, QColor(fg)));
    m_search->setStyleSheet(QStringLiteral("QLineEdit { background:%1; }").arg(light ? "#dddddd" : "#151515"));
    for (int i = 0; i < 9; ++i)
    {
        const QColor color(i == 0 && light ? "#e8851a" : swatchColors[i]);
        const QColor background(well);
        const QColor dim(qRound(color.red() * .7 + background.red() * .3),
                         qRound(color.green() * .7 + background.green() * .3),
                         qRound(color.blue() * .7 + background.blue() * .3));
        // Native orange uses BG/Accent Bright; other selections reuse VLC's
        // existing authored family derivation rather than inventing shades.
        const QColor selectedWell = selectedAccentWell(color, light);
        m_swatches[i]->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; border:6px solid %2; border-radius:4px; padding:0; }"
            "QPushButton:checked { background:%3; border-color:%4; } QPushButton:focus { border-color:%5; }")
            .arg(dim.name(), well, color.name(), selectedWell.name(), focus));
    }
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_search || watched == m_results) && event->type() == QEvent::KeyPress)
    {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
        {
            // Navigation can hide the tree synchronously. Consume this key
            // so QDialog cannot also click a newly focused default button.
            if (m_pages->currentIndex() == 1 && m_results->topLevelItemCount() > 0)
            {
                auto *item = watched == m_results ? m_results->currentItem() : m_results->topLevelItem(0);
                if (item)
                {
                    const QVariant category = item->data(0, Qt::UserRole);
                    if (category.isValid())
                    {
                        m_search->clear();
                        selectCategory(category.toInt());
                    }
                    else
                    {
                        m_results->setCurrentItem(item);
                        m_results->setFocus();
                    }
                }
            }
            return true;
        }
    }
    return QVLCDialog::eventFilter(watched, event);
}

void SettingsDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->position().y() < 50 && windowHandle())
        windowHandle()->startSystemMove();
    QVLCDialog::mousePressEvent(event);
}
