/*****************************************************************************
 * settings_dialog.cpp : Authored settings shell and General / Interface editors
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
#include <vlc_plugin.h>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSettings>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_player.h>
#include <QCompleter>
#include <QColorDialog>
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace {
// Retain QPushButton keyboard / accessibility semantics while drawing the
// authored 68x32 switch, rather than borrowing a platform checkbox image.
class SettingsToggle : public QPushButton
{
public:
    using QPushButton::QPushButton;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool light = property("light").toBool();
        painter.setPen(Qt::NoPen);
        painter.setBrush(isChecked() ? property("accentWell").value<QColor>() : QColor(light ? "#dedede" : "#181818"));
        painter.drawRoundedRect(rect(), 4, 4);
        painter.setBrush(isChecked() ? property("accentDim").value<QColor>() : QColor(light ? "#b3b3b3" : "#404040"));
        painter.drawRoundedRect(QRectF(isChecked() ? 39 : 3, 3, 26, 26), 3, 3);
        painter.setPen(isChecked() ? (light ? QColor("#3b3b3b") : property("accentMain").value<QColor>()) : QColor("#6b6b6b"));
        QFont font = this->font();
        font.setPixelSize(12);
        painter.setFont(font);
        painter.drawText(QRect(isChecked() ? 0 : 29, 0, 39, 32), Qt::AlignCenter, text());
        if (hasFocus())
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(property("accentMain").value<QColor>());
            painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), 4, 4);
        }
    }
};
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
    body->setContentsMargins(8, 8, 8, 8);
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
    connect(all, &QPushButton::clicked, this, [this] { openAllSettings(); });
    body->addWidget(navigationScroll);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(0, 12, 0, 8);
    content->setSpacing(16);
    m_pages = new QStackedWidget;
    auto *categoryPage = new QWidget;
    auto *panel = new QVBoxLayout(categoryPage);
    panel->setContentsMargins(0, 0, 0, 0);
    panel->setSpacing(0);
    m_accentRow = new QWidget;
    auto *accentLayout = new QVBoxLayout(m_accentRow);
    accentLayout->setContentsMargins(0, 0, 0, 0);
    accentLayout->setSpacing(0);
    auto *appearance = label(qtr("Appearance"));
    appearance->setObjectName("settingsSection");
    appearance->setFixedHeight(39);
    accentLayout->addWidget(appearance);
    // VLC has no cross-platform config key for the translation language.
    // Preserve the frame's row as explanatory text, not a fictional editor.
#ifdef _WIN32
    auto *language = label(qtr("Interface language — set in All settings"));
#else
    auto *language = label(qtr("Interface language — system locale"));
#endif
    language->setFixedHeight(46);
    language->setToolTip(qtr("Translation language follows the system locale on Linux and macOS. The Windows language selector remains in All settings."));
    accentLayout->addWidget(language);
    auto *themeRow = new QHBoxLayout;
    themeRow->setContentsMargins(0, 5, 0, 5);
    themeRow->addWidget(label(qtr("Theme")), 1);
    m_theme = new QComboBox;
    m_theme->setObjectName("settingsTheme");
    m_theme->setAccessibleName(qtr("Theme"));
    m_theme->addItem(qtr("Dark"), 2);
    m_theme->addItem(qtr("Light"), 1);
    m_theme->addItem(qtr("System"), 0);
    m_theme->setFixedSize(320, 36);
    m_savedTheme = p_intf->p_mi ? p_intf->p_mi->getColorScheme()->currentIndex() : 0;
    m_theme->setCurrentIndex(m_theme->findData(m_savedTheme));
    m_theme->setEnabled(p_intf->p_mi != nullptr);
    themeRow->addWidget(m_theme);
    accentLayout->addLayout(themeRow);
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
    m_accentValue->hide(); // canonical key and restart status are in the tooltip/footer
    accentLayout->addWidget(m_accentValue);
    m_accentRow->setEnabled(accent && !m_defaultAccent.isEmpty());
    panel->addWidget(m_accentRow);
    const auto specs = settingsControlSpecs();
    for (int category = 0; category < 13; ++category)
    {
        for (bool advanced : {false, true})
        {
            auto *form = new QWidget;
            auto *formLayout = new QVBoxLayout(form);
            formLayout->setContentsMargins(0, 0, 0, 0);
            formLayout->setSpacing(0);
            QString section;
            for (const auto &spec : specs)
            {
                if (spec.category != category || spec.advanced != advanced || !config_FindConfig(spec.key))
                    continue;
                if (QString::fromLatin1(spec.section) == "Skin" && !module_exists("skins2"))
                    continue;
                if (section != QString::fromUtf8(spec.section))
                {
                    section = QString::fromUtf8(spec.section);
                    auto *heading = label(qfut(spec.section));
                    heading->setObjectName("settingsSection");
                    const bool firstSection = formLayout->count() == 0 && category != 0 && !advanced;
                    heading->setFixedHeight(firstSection ? 39 : 63);
                    heading->setContentsMargins(0, firstSection ? 0 : 24, 0, 0);
                    formLayout->addWidget(heading);
                }
                addControl(spec, formLayout);
                if (QString::fromLatin1(spec.key) == "aout")
                {
                    auto *deviceRow = new QWidget;
                    auto *deviceLayout = new QHBoxLayout(deviceRow);
                    deviceLayout->setContentsMargins(0, 5, 0, 5);
                    const QString caption = qtr("Output device (current session)");
                    auto *deviceLabel = label(caption);
                    deviceLayout->addWidget(deviceLabel, 1);
                    m_device = new QComboBox;
                    m_device->setObjectName("settingsAudioDevice");
                    m_device->setAccessibleName(caption);
                    m_device->setFixedSize(320, 36);
                    m_aout = vlc_player_aout_Hold(p_intf->p_player);
                    if (m_aout)
                    {
                        char *current = aout_DeviceGet(m_aout);
                        m_savedDevice = QString::fromUtf8(current ? current : "");
                        free(current);
                        char **ids = nullptr, **names = nullptr;
                        const int count = aout_DevicesList(m_aout, &ids, &names);
                        for (int i = 0; i < count; ++i)
                        {
                            m_device->addItem(QString::fromUtf8(names[i]), QString::fromUtf8(ids[i]));
                            free(ids[i]);
                            free(names[i]);
                        }
                        free(ids);
                        free(names);
                    }
                    m_device->setEnabled(m_device->count() > 0);
                    if (m_device->isEnabled()) setEditorValue(m_device, m_savedDevice);
                    else m_device->addItem(qtr("No device enumeration from current output"), m_savedDevice);
                    m_device->setToolTip(qtr("aout_DeviceSet — current output only. Staged until Save; not stored in vlcrc. Restart-needed output module changes do not alter this list. Reopen Settings after starting audio to refresh devices."));
                    deviceLabel->setBuddy(m_device);
                    deviceLayout->addWidget(m_device);
                    deviceRow->setFixedHeight(46);
                    formLayout->addWidget(deviceRow);
                }
            }
            if (category == 7 && advanced)
            {
                auto *duration = label(qtr("Subtitle Duration factor — unavailable: subsdelay asserts in this engine."));
                duration->setObjectName("settingsSubtitleDurationUnavailable");
                formLayout->addWidget(duration);
            }
            (advanced ? m_advancedForms : m_simpleForms)[category] = form;
            if (!advanced)
                panel->addWidget(form);
        }
    }
    m_pending = label(QString());
    m_pending->setObjectName("settingsPending");
    panel->addWidget(m_pending);
    m_advancedButton = new QPushButton;
    m_advancedButton->setObjectName("settingsAdvanced");
    m_advancedButton->setFixedHeight(38);
    panel->addWidget(m_advancedButton);
    for (auto *form : m_advancedForms)
        if (form) panel->addWidget(form);
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
    auto *searchHelp = label(qtr("Select a result to open its control. Options not yet migrated open in the original preferences."));
    searchLayout->addWidget(searchHelp);
    auto *legacy = new QPushButton(qtr("Open All settings"));
    searchLayout->addWidget(legacy);
    connect(legacy, &QPushButton::clicked, this, [this] { openAllSettings(); });
    m_pages->addWidget(searchPage);
    m_search->installEventFilter(this);
    m_results->installEventFilter(this);
    connect(m_results, &QTreeWidget::itemClicked, this, &SettingsDialog::activateResult);
    connect(m_results, &QTreeWidget::itemActivated, this, &SettingsDialog::activateResult);
    // One scroll area owns the stack. Its native bar remains the scroll model
    // (wheel, keyboard focus and ensureWidgetVisible all use it), but its visible
    // rail lives in the shell gutter, not on top of or inside the native column.
    m_pageScroll = new QScrollArea;
    m_pageScroll->setObjectName("settingsPageScroll");
    m_pageScroll->setFrameShape(QFrame::NoFrame);
    m_pageScroll->setWidgetResizable(true);
    m_pageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pageScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pageScroll->setWidget(m_pages);
    m_pageScroll->viewport()->setAutoFillBackground(false);
    m_pages->setAutoFillBackground(false);
    auto *scrollRow = new QHBoxLayout;
    scrollRow->setContentsMargins(0, 0, 0, 0);
    scrollRow->setSpacing(0);
    scrollRow->addWidget(m_pageScroll, 1);
    auto *gutter = new QWidget;
    gutter->setFixedWidth(16);
    auto *gutterLayout = new QHBoxLayout(gutter);
    gutterLayout->setContentsMargins(6, 0, 4, 0);
    auto *pageBar = new QScrollBar(Qt::Vertical);
    pageBar->setObjectName("settingsPageScrollBar");
    pageBar->setFixedWidth(6);
    pageBar->setFocusPolicy(Qt::NoFocus);
    pageBar->hide();
    gutterLayout->addWidget(pageBar);
    scrollRow->addWidget(gutter);
    auto *pageVertical = m_pageScroll->verticalScrollBar();
    connect(pageVertical, &QScrollBar::rangeChanged, pageBar, [pageVertical, pageBar](int min, int max) {
        pageBar->setRange(min, max);
        pageBar->setPageStep(pageVertical->pageStep());
        pageBar->setVisible(max > min);
    });
    connect(pageVertical, &QScrollBar::valueChanged, pageBar, &QScrollBar::setValue);
    connect(pageBar, &QScrollBar::valueChanged, pageVertical, &QScrollBar::setValue);
    content->addLayout(scrollRow, 1);
    m_restart = label(QString());
    m_restart->setObjectName("settingsRestart");
    content->addWidget(m_restart);
    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 16, 0);
    auto *reset = new QPushButton(qtr("Reset General / Interface"));
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
    connect(save, &QPushButton::clicked, this, [this] { if (saveChanges()) accept(); });
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetEditors);
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

SettingsDialog::~SettingsDialog()
{
    if (m_aout) aout_Release(m_aout);
}

void SettingsDialog::selectCategory(int category)
{
    if (category != m_category)
        m_pageScroll->verticalScrollBar()->setValue(0);
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
    const bool migrated = category == 0 || category == 1 || category == 4 || category == 6 || category == 7;
    m_pending->setVisible(!migrated);
    for (int i = 0; i < 13; ++i)
    {
        m_simpleForms[i]->setVisible(i == category);
        m_advancedForms[i]->setVisible(i == category && m_advanced[category]);
    }
    m_pending->setText(qtr("%1 — %2 panel pending.\nNo category controls have been migrated yet. All existing options remain available in All settings.")
        .arg(qfut(categoryNames[category]), m_advanced[category] ? qtr("Advanced") : qtr("Simple")));
    findChild<QPushButton *>(QStringLiteral("settingsReset"))->setText(category <= 1
        ? qtr("Reset General / Interface") : qtr("Reset %1").arg(qfut(categoryNames[category])));
    findChild<QPushButton *>(QStringLiteral("settingsReset"))->setEnabled(migrated);
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
        auto *item = new QTreeWidgetItem(m_results, {qfut(categoryNames[i]), qtr("Category")});
        item->setData(0, Qt::UserRole, i);
        ++categories;
    }
    for (const auto &option : m_metadata)
    {
        if (!settingsOptionMatches(option, query))
            continue;
        auto *item = new QTreeWidgetItem(m_results, {option.label + "\n" + option.key, option.module + "\n" + option.subcategory});
        item->setToolTip(0, option.description);
        item->setData(0, Qt::UserRole + 1, option.key);
        item->setData(0, Qt::UserRole + 2, option.accentSwatch);
        ++options;
    }
    m_resultCount->setText(qtr("%1 options · %2 categories — labels and real configuration keys").arg(options).arg(categories));
    // A tall category can leave the shared scroll owner far below its top.
    // Search must expose its result header/rows for mouse activation as well
    // as Enter. Settle the stack's layout before resetting its scroll model.
    QTimer::singleShot(0, this, [this] {
        if (m_pages->currentIndex() == 1)
        {
            m_pages->layout()->activate();
            m_pageScroll->verticalScrollBar()->setValue(0);
        }
    });
}

void SettingsDialog::activateResult(QTreeWidgetItem *item)
{
    if (!item || m_pages->currentIndex() != 1)
        return;
    // Copy the result before clear() synchronously deletes its tree item.
    const QVariant category = item->data(0, Qt::UserRole);
    const QString key = item->data(0, Qt::UserRole + 1).toString();
    const int swatch = item->data(0, Qt::UserRole + 2).toInt();
    if (category.isValid())
    {
        m_search->clear();
        selectCategory(category.toInt());
        m_categories[category.toInt()]->setFocus(Qt::OtherFocusReason);
        return;
    }
    QWidget *target = nullptr;
    int page = 0;
    bool advanced = false;
    if (key == QStringLiteral("MainWindow/color-scheme-index"))
        target = m_theme;
    else if (key == QStringLiteral("aout_DeviceSet"))
    {
        target = m_device;
        page = 6;
    }
    else if (key == QStringLiteral("qt-accent-color"))
        target = m_swatches[swatch >= 0 && swatch < 9 ? swatch : 0];
    else
        for (const auto &spec : settingsControlSpecs())
            if (key == QString::fromLatin1(spec.key))
                for (const auto &editor : m_editors)
                    if (editor.key == key)
                    {
                        target = editor.widget;
                        page = spec.category;
                        advanced = spec.advanced;
                    }
    if (!target)
    {
        openAllSettings(key);
        return;
    }
    m_advanced[page] |= advanced;
    m_search->clear();
    selectCategory(page);
    // Layout requests from showing the Advanced form must settle before scroll.
    QTimer::singleShot(0, this, [this, target] {
        m_pages->layout()->activate();
        m_pageScroll->ensureWidgetVisible(target, 8, 24);
        target->setFocus(Qt::OtherFocusReason);
    });
}

void SettingsDialog::updateAccent()
{
    for (int i = 0; i < 9; ++i)
        m_swatches[i]->setChecked(m_accent.compare(i == 0 ? m_defaultAccent : QString::fromLatin1(swatchColors[i]), Qt::CaseInsensitive) == 0);
    m_accentValue->setText(qtr("qt-accent-color: %1 · Restart needed after saving").arg(m_accent));
    m_restart->setText(qtr("Config changes require restarting VLC. Theme / current audio device apply on Save."));
}

bool SettingsDialog::hasChanges() const
{
    if ((m_device && m_device->currentData().toString() != m_savedDevice) ||
        m_accent != m_savedAccent || m_theme->currentData().toInt() != m_savedTheme)
        return true;
    return std::any_of(m_editors.cbegin(), m_editors.cend(), [this](const Editor &editor) {
        return editorValue(editor.widget) != editor.saved;
    });
}

bool SettingsDialog::saveChanges()
{
    if (!hasChanges())
        return true;
    QVector<QVariant> values;
    QVector<QVariant> previous;
    bool configChanged = m_accent != m_savedAccent;
    for (const auto &editor : m_editors)
    {
        auto value = editorValue(editor.widget);
        const auto *metadata = config_FindConfig(qtu(editor.key));
        if (editor.type == CONFIG_ITEM_INTEGER || editor.type == CONFIG_ITEM_RGB)
        {
            bool ok;
            const auto number = value.toString().toLongLong(&ok);
            if (!ok || number < metadata->min.i || number > metadata->max.i)
            {
                QMessageBox::warning(this, qtr("Invalid setting"), qtr("%1 must be an integer between %2 and %3.")
                    .arg(editor.key).arg(metadata->min.i).arg(metadata->max.i));
                editor.widget->setFocus();
                return false;
            }
            value = QVariant::fromValue<qlonglong>(number);
        }
        values.push_back(value);
        configChanged |= value != editor.saved;
        if (IsConfigStringType(editor.type))
        {
            char *text = config_GetPsz(qtu(editor.key));
            previous.push_back(QString::fromUtf8(text ? text : ""));
            free(text);
        }
        else if (editor.type == CONFIG_ITEM_FLOAT)
            previous.push_back(config_GetFloat(qtu(editor.key)));
        else
            previous.push_back(QVariant::fromValue<qlonglong>(config_GetInt(qtu(editor.key))));
    }
    auto put = [](const Editor &editor, const QVariant &value) {
        if (IsConfigStringType(editor.type))
            config_PutPsz(qtu(editor.key), qtu(value.toString()));
        else if (editor.type == CONFIG_ITEM_FLOAT)
            config_PutFloat(qtu(editor.key), value.toFloat());
        else
            config_PutInt(qtu(editor.key), value.toLongLong());
    };
    const bool deviceChanged = m_device && m_device->currentData().toString() != m_savedDevice;
    if (deviceChanged)
    {
        audio_output_t *current = vlc_player_aout_Hold(p_intf->p_player);
        const bool same = current == m_aout;
        if (current) aout_Release(current);
        // aout_DeviceSet() selects the default device with NULL, not "".
        const QByteArray device = m_device->currentData().toString().toUtf8();
        if (!same || aout_DeviceSet(m_aout, device.isEmpty() ? nullptr : device.constData()) != 0)
        {
            QMessageBox::warning(this, qtr("Cannot select audio device"),
                qtr("The audio output changed or rejected the device. Reopen Settings to refresh available devices. No preferences were written."));
            return false;
        }
    }
    const QString previousAccent = configAccent();
    for (int i = 0; i < m_editors.size(); ++i)
        if (values[i] != m_editors[i].saved)
            put(m_editors[i], values[i]);
    if (m_accent != m_savedAccent)
        config_PutPsz("qt-accent-color", qtu(m_accent));
    if (configChanged && config_SaveConfigFile(p_intf) != 0)
    {
        for (int i = 0; i < m_editors.size(); ++i)
            if (values[i] != m_editors[i].saved)
                put(m_editors[i], previous[i]);
        config_PutPsz("qt-accent-color", qtu(previousAccent));
        if (deviceChanged) aout_DeviceSet(m_aout, qtu(m_savedDevice));
        QMessageBox::warning(this, qtr("Cannot save Configuration"), qtr("Preferences file could not be saved. Changes were not applied."));
        return false;
    }
    // Theme is an existing Qt settings API, not a made-up vlcrc option.
    if (m_theme->currentData().toInt() != m_savedTheme && p_intf->p_mi)
    {
        auto *settings = getSettings();
        const QString key = QStringLiteral("MainWindow/color-scheme-index");
        const QVariant previousTheme = settings->value(key);
        settings->setValue(key, m_theme->currentData());
        settings->sync();
        if (settings->status() != QSettings::NoError)
        {
            if (previousTheme.isValid()) settings->setValue(key, previousTheme);
            else settings->remove(key);
            for (int i = 0; i < m_editors.size(); ++i)
                if (values[i] != m_editors[i].saved) put(m_editors[i], previous[i]);
            config_PutPsz("qt-accent-color", qtu(previousAccent));
            if (deviceChanged) aout_DeviceSet(m_aout, qtu(m_savedDevice));
            const int rollback = configChanged ? config_SaveConfigFile(p_intf) : 0;
            QMessageBox::warning(this, qtr("Cannot save Configuration"), rollback == 0
                ? qtr("Qt theme preferences could not be saved. Changes were rolled back.")
                : qtr("Qt theme preferences could not be saved, and the VLC configuration rollback also failed. Check file permissions before retrying."));
            return false;
        }
        p_intf->p_mi->getColorScheme()->setCurrentIndex(m_theme->currentData().toInt());
    }
    for (int i = 0; i < m_editors.size(); ++i)
        m_editors[i].saved = editorValue(m_editors[i].widget);
    if (m_device) m_savedDevice = m_device->currentData().toString();
    m_savedAccent = m_accent;
    m_savedTheme = m_theme->currentData().toInt();
    updateAccent();
    return true;
}

void SettingsDialog::resetEditors()
{
    const QString scope = m_category <= 1 ? qtr("General / Interface") : qfut(categoryNames[m_category]);
    if (QMessageBox::question(this, qtr("Reset %1").arg(scope),
        qtr("Restore only the controls in %1 to their declared defaults? Press Save to persist them; Cancel discards this reset. Other VLC preferences are not reset.").arg(scope),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok)
        return;
    for (const auto &editor : m_editors)
        if (m_category <= 1 ? editor.category <= 1 : editor.category == m_category)
            setEditorValue(editor.widget, editor.defaults);
    if (m_category <= 1)
    {
        m_accent = m_defaultAccent;
        m_theme->setCurrentIndex(m_theme->findData(0)); // existing MainCtx QSettings default
    }
    // The output device is a live, session-only selection (not a vlcrc key) and
    // not every output can move back to an unnamed default (PulseAudio rejects
    // it), so a category Reset leaves the current device untouched.
    updateAccent();
}

void SettingsDialog::openAllSettings(const QString &key)
{
    if (hasChanges())
    {
        const auto response = QMessageBox::question(this, qtr("Unsaved settings"),
            qtr("Save changes before opening the original VLC preferences?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (response == QMessageBox::Cancel || (response == QMessageBox::Save && !saveChanges()))
            return;
    }
    emit allSettingsRequested(key);
}

QVariant SettingsDialog::editorValue(QWidget *widget) const
{
    if (auto *combo = qobject_cast<QComboBox *>(widget))
        return combo->currentData();
    if (auto *toggle = qobject_cast<QPushButton *>(widget))
        return QVariant::fromValue<qlonglong>(toggle->isChecked());
    if (auto *number = qobject_cast<QDoubleSpinBox *>(widget))
        return number->value();
    const QString text = qobject_cast<QLineEdit *>(widget)->text();
    if (widget->property("rgb").toBool())
    {
        const QColor color(text);
        if (color.isValid() && color.alpha() == 255)
            return QVariant::fromValue<qlonglong>(color.rgb() & 0xFFFFFF);
        return text; // invalid text is rejected before any config write
    }
    if (widget->property("integer").toBool())
    {
        bool ok;
        const auto number = text.toLongLong(&ok);
        if (ok) return QVariant::fromValue<qlonglong>(number);
    }
    return text;
}

void SettingsDialog::setEditorValue(QWidget *widget, const QVariant &value)
{
    if (auto *combo = qobject_cast<QComboBox *>(widget))
    {
        int index = combo->findData(value);
        if (index < 0)
        {
            combo->addItem(qtr("Current value: %1").arg(value.toString()), value);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }
    else if (auto *toggle = qobject_cast<QPushButton *>(widget))
    {
        toggle->setChecked(value.toBool());
        toggle->setText(value.toBool() ? qtr("ON") : qtr("OFF"));
    }
    else if (auto *number = qobject_cast<QDoubleSpinBox *>(widget))
        number->setValue(value.toDouble());
    else
        qobject_cast<QLineEdit *>(widget)->setText(widget->property("rgb").toBool()
            ? QColor::fromRgb(value.toUInt()).name() : value.toString());
}

void SettingsDialog::addControl(const SettingsControlSpec &spec, QVBoxLayout *layout)
{
    const auto *metadata = config_FindConfig(spec.key);
    auto *row = new QWidget;
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 5, 0, 5);
    rowLayout->setSpacing(10);
    const QString title = *spec.units ? qtr("%1 (%2)").arg(qfut(spec.label), qfut(spec.units)) : qfut(spec.label);
    auto *caption = label(title);
    rowLayout->addWidget(caption, 1);
    const int type = metadata->i_type;
    QVariant current, defaults;
    if (IsConfigStringType(type))
    {
        char *value = config_GetPsz(spec.key);
        current = QString::fromUtf8(value ? value : "");
        free(value);
        defaults = QString::fromUtf8(metadata->orig.psz ? metadata->orig.psz : "");
    }
    else if (type == CONFIG_ITEM_FLOAT)
    {
        current = config_GetFloat(spec.key);
        defaults = metadata->orig.f;
    }
    else
    {
        current = QVariant::fromValue<qlonglong>(config_GetInt(spec.key));
        defaults = QVariant::fromValue<qlonglong>(metadata->orig.i);
    }
    QWidget *widget;
    if (type == CONFIG_ITEM_BOOL)
    {
        auto *toggle = new SettingsToggle;
        toggle->setProperty("settingsToggle", true);
        toggle->setCheckable(true);
        toggle->setFixedSize(68, 32);
        connect(toggle, &QPushButton::toggled, toggle, [toggle](bool on) { toggle->setText(on ? qtr("ON") : qtr("OFF")); });
        widget = toggle;
    }
    else if (type != CONFIG_ITEM_RGB && (type == CONFIG_ITEM_MODULE || metadata->list_count || QString::fromLatin1(spec.key) == "qt-fullscreen-screennumber" || QString::fromLatin1(spec.key) == "intf"))
    {
        auto *combo = new QComboBox;
        for (unsigned i = 0; i < metadata->list_count; ++i)
        {
            // The core declaration survives builds without the provider;
            // mirror aout's real capability gate rather than offer a no-op.
            if (QString::fromLatin1(spec.key) == "mix-mode" &&
                metadata->list.i[i] == AOUT_MIX_MODE_BINAURAL && !module_exists("spatialaudio"))
                continue;
            combo->addItem(qfut(metadata->list_text[i]), IsConfigStringType(type)
                ? QVariant(QString::fromUtf8(metadata->list.psz[i]))
                : QVariant::fromValue<qlonglong>(metadata->list.i[i]));
        }
        if (type == CONFIG_ITEM_MODULE)
        {
            combo->addItem(qtr("Automatic"), QStringLiteral("any"));
            if (QString::fromLatin1(spec.key) == "audio-visual")
                combo->addItem(qtr("Disabled"), QStringLiteral("none"));
            size_t count = 0;
            module_t **modules = module_list_get(&count);
            for (size_t i = 0; i < count; ++i)
                if (module_provides(modules[i], metadata->psz_type))
                    combo->addItem(qfut(module_GetLongName(modules[i])), QString::fromUtf8(module_get_object(modules[i])));
            module_list_free(modules);
        }
        if (QString::fromLatin1(spec.key) == "qt-fullscreen-screennumber")
        {
            combo->addItem(qtr("Current screen"), QVariant::fromValue<qlonglong>(-1));
            const auto screens = QGuiApplication::screens();
            for (int i = 0; i < screens.size(); ++i)
                combo->addItem(screens[i]->name(), QVariant::fromValue<qlonglong>(i));
        }
        if (QString::fromLatin1(spec.key) == "intf")
        {
            combo->addItem(qtr("Default interface"), QString());
            combo->addItem(qtr("Qt"), QStringLiteral("qt"));
            combo->addItem(qtr("Custom skin (separate interface)"), QStringLiteral("skins2"));
        }
        combo->setFixedSize(320, 36);
        widget = combo;
    }
    else if (type == CONFIG_ITEM_FLOAT)
    {
        auto *number = new QDoubleSpinBox;
        number->setDecimals(3);
        number->setRange(metadata->min.f, metadata->max.f);
        number->setSingleStep(.05);
        number->setFixedSize(320, 36);
        widget = number;
    }
    else
    {
        auto *text = new QLineEdit;
        text->setProperty("integer", type == CONFIG_ITEM_INTEGER);
        text->setProperty("rgb", type == CONFIG_ITEM_RGB);
        text->setFixedSize(320, 36);
        if (type == CONFIG_ITEM_MODULE_LIST)
        {
            QStringList modules;
            size_t count = 0;
            module_t **installed = module_list_get(&count);
            for (size_t i = 0; i < count; ++i)
                if (module_provides(installed[i], metadata->psz_type))
                    modules.push_back(QString::fromUtf8(module_get_object(installed[i])));
            module_list_free(installed);
            auto *completion = new QCompleter(modules, text);
            completion->setCaseSensitivity(Qt::CaseInsensitive);
            text->setCompleter(completion);
            text->setPlaceholderText(qtr("Colon-separated module chain"));
            caption->setToolTip(qtr("Installed modules: %1").arg(modules.join(", ")));
            text->setProperty("installedModules", modules.join(", "));
        }
        widget = text;
    }
    widget->setObjectName(QStringLiteral("settingsKey_%1").arg(QString::fromLatin1(spec.key)));
    widget->setAccessibleName(title);
    QString tooltip = QString::fromLatin1(spec.key) + qtr(" — restart needed after saving") + "\n" + qfut(metadata->psz_longtext ? metadata->psz_longtext : "");
    if (*spec.units)
        tooltip += "\n" + qtr("Units: %1").arg(qfut(spec.units));
    if (type == CONFIG_ITEM_INTEGER && !metadata->list_count)
        tooltip += "\n" + qtr("Declaration range: %1 to %2").arg(metadata->min.i).arg(metadata->max.i);
    if (type == CONFIG_ITEM_FLOAT)
        tooltip += "\n" + qtr("Declaration range: %1 to %2").arg(metadata->min.f).arg(metadata->max.f);
    if (widget->property("installedModules").isValid())
        tooltip += "\n" + qtr("Installed modules: %1").arg(widget->property("installedModules").toString());
    caption->setToolTip(tooltip);
    widget->setToolTip(tooltip);
    caption->setBuddy(widget);
    setEditorValue(widget, current);
    if (QString::fromLatin1(spec.key) == "mix-mode" && !module_exists("spatialaudio"))
    {
        auto *combo = qobject_cast<QComboBox *>(widget);
        const int unavailable = combo->findData(QVariant::fromValue<qlonglong>(AOUT_MIX_MODE_BINAURAL));
        if (unavailable >= 0) // Preserve an existing value, but do not offer it.
            qobject_cast<QStandardItemModel *>(combo->model())->item(unavailable)->setEnabled(false);
        widget->setToolTip(tooltip + "\n" + qtr("Binaural mixing is unavailable: spatialaudio is not installed."));
    }
    rowLayout->addWidget(widget);
    if (type == CONFIG_ITEM_SAVEFILE || type == CONFIG_ITEM_DIRECTORY || QString::fromLatin1(spec.key) == "skins2-last")
    {
        widget->setFixedWidth(210);
        auto *browse = new QPushButton(qtr("Browse"));
        browse->setFixedSize(100, 36);
        browse->setAccessibleName(qtr("Browse for %1").arg(qfut(spec.label)));
        connect(browse, &QPushButton::clicked, this, [this, widget, type] {
            const QString file = type == CONFIG_ITEM_SAVEFILE
                ? QFileDialog::getSaveFileName(this, qtr("Log file"), editorValue(widget).toString())
                : type == CONFIG_ITEM_DIRECTORY
                    ? QFileDialog::getExistingDirectory(this, qtr("Select folder"), editorValue(widget).toString())
                    : QFileDialog::getOpenFileName(this, qtr("Skin file"), editorValue(widget).toString());
            if (!file.isEmpty()) setEditorValue(widget, file);
        });
        rowLayout->addWidget(browse);
    }
    if (type == CONFIG_ITEM_RGB)
    {
        widget->setFixedWidth(210);
        auto *choose = new QPushButton(qtr("Color"));
        choose->setFixedSize(100, 36);
        choose->setAccessibleName(qtr("Choose %1").arg(title));
        connect(choose, &QPushButton::clicked, this, [this, widget, title] {
            const auto color = QColorDialog::getColor(QColor::fromRgb(editorValue(widget).toUInt()), this, title);
            if (color.isValid()) setEditorValue(widget, color.rgb() & 0xFFFFFF);
        });
        rowLayout->addWidget(choose);
    }
    row->setFixedHeight(type == CONFIG_ITEM_BOOL ? 42 : 46);
    layout->addWidget(row);
    m_editors.push_back({spec.category, QString::fromLatin1(spec.key), type, widget, editorValue(widget), defaults});
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
    for (const auto &editor : m_editors)
    {
        if (editor.type != CONFIG_ITEM_BOOL) continue;
        editor.widget->setProperty("light", light);
        editor.widget->setProperty("accentMain", m_palette->get_accentMain());
        editor.widget->setProperty("accentDim", m_palette->get_accentDim());
        editor.widget->setProperty("accentWell", selectedAccentWell(m_palette->get_accentMain(), light));
        editor.widget->update();
    }
    setStyleSheet(QStringLiteral(
        "QWidget { color:%1; font-family:Inter; font-size:14px; }"
        "QWidget#settingsSurface { background:%2; border-radius:14px; }"
        "QWidget#settingsHeader { background:%3; border-top-left-radius:14px; border-top-right-radius:14px; }"
        "QWidget#settingsNavigation { background:%4; border-radius:6px; }"
        "QWidget#settingsSwatches { background:%3; border-radius:6px; }"
        "QLabel { background:transparent; }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background:%2; border:0; }"
        "QComboBox, QDoubleSpinBox { background:%4; border:1px solid %3; border-radius:4px; padding:4px 12px; }"
        "QDoubleSpinBox { background:%3; }"
        "QComboBox:focus, QDoubleSpinBox:focus { border:1px solid %7; }"
        "QScrollBar:vertical { background:%2; width:6px; margin:0; }"
        "QScrollBar::handle:vertical { background:%1; min-height:32px; border-radius:3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; border:0; background:transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"
        "QComboBox QAbstractItemView { background:%4; color:%1; selection-background-color:%6; }"
        "QPushButton[settingsToggle=true] { background:%3; padding:0; }"
        "QPushButton[settingsToggle=true]:checked { background:%7; color:%2; }"
        "QLabel#settingsTitle, QLabel#settingsSection { font-size:16px; }"
        "QLabel#settingsSection { color:%5; }"
        "QPushButton { background:%4; border:1px solid transparent; border-radius:4px; padding:8px 12px; }"
        "QPushButton:hover, QPushButton:checked { background:%6; color:%5; }"
        "QPushButton:focus { border:1px solid %7; }"
        "QPushButton[category=true] { text-align:left; padding:0px 8px; }"
        "QPushButton#settingsClose { background:transparent; padding:0; }"
        "QLineEdit { background:%3; border:1px solid transparent; border-radius:6px; padding:6px 10px; }"
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
                    activateResult(item);
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
