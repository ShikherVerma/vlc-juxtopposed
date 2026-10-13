/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "systempalette.hpp"
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <maininterface/mainctx.hpp>
#include <vlc_modules.h>
#include <vlc_picture.h>

namespace {

QColor setColorAlpha(const QColor& c1, float alpha)
{
    QColor c(c1);
    c.setAlphaF(alpha);
    return c;
}

#ifndef _WIN32
/**
 * function taken from QtBase gui/platform/unix/qgenericunixservices.cpp
 */
static inline QByteArray detectLinuxDesktopEnvironment()
{
    const QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (!xdgCurrentDesktop.isEmpty())
        return xdgCurrentDesktop.toUpper(); // KDE, GNOME, UNITY, LXDE, MATE, XFCE...

    // Classic fallbacks
    if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))
        return QByteArrayLiteral("KDE");
    if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID"))
        return QByteArrayLiteral("GNOME");

    // Fallback to checking $DESKTOP_SESSION (unreliable)
    QByteArray desktopSession = qgetenv("DESKTOP_SESSION");

    // This can be a path in /usr/share/xsessions
    int slash = desktopSession.lastIndexOf('/');
    if (slash != -1) {
        QSettings desktopFile(QFile::decodeName(desktopSession + ".desktop"), QSettings::IniFormat);
        desktopFile.beginGroup(QStringLiteral("Desktop Entry"));
        QByteArray desktopName = desktopFile.value(QStringLiteral("DesktopNames")).toByteArray();
        if (!desktopName.isEmpty())
            return desktopName;

        // try decoding just the basename
        desktopSession = desktopSession.mid(slash + 1);
    }

    if (desktopSession == "gnome")
        return QByteArrayLiteral("GNOME");
    else if (desktopSession == "xfce")
        return QByteArrayLiteral("XFCE");
    else if (desktopSession == "kde")
        return QByteArrayLiteral("KDE");

    return QByteArrayLiteral("UNKNOWN");
}

bool isGTKBasedEnvironment()
{
    QList<QByteArray> gtkBasedEnvironments{
        "GNOME",
        "X-CINNAMON",
        "UNITY",
        "MATE",
        "XFCE",
        "LXDE"
    };
    const QList<QByteArray> desktopNames = detectLinuxDesktopEnvironment().split(':');
    for (const QByteArray& desktopName: desktopNames)
    {
        if (gtkBasedEnvironments.contains(desktopName))
            return true;
    }
    return false;
}
#endif


static void PaletteChangedCallback(vlc_qt_theme_provider_t*, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    emit priv->paletteChanged();
}

static void MetricsChangedCallback(vlc_qt_theme_provider_t*, vlc_qt_theme_image_type type, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    assert(priv);
    priv->updateMetrics(type);
}


static void ReleaseVLCPictureCb(void* data)
{
    auto pic = static_cast<picture_t*>(data);
    if (pic)
        picture_Release(pic);
}


static void setColorRBGAInt(
    vlc_qt_theme_provider_t* obj,
    vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
    vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
    int r, int g, int b, int a)
{
    auto palette = static_cast<SystemPalette*>(obj->setColorData);
    QColor color(r,g,b,a);
    palette->setColor(
        static_cast<ColorContext::ColorSet>(set), static_cast<ColorContext::ColorSection>(section),
        static_cast<ColorContext::ColorName>(name), static_cast<ColorContext::ColorState>(state),
        color);
}

static void setColorRBGAFloat(
    vlc_qt_theme_provider_t* obj,
    vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
    vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
    double r, double g, double b, double a)
{
    auto palette = static_cast<SystemPalette*>(obj->setColorData);
    QColor color;
    color.setRgbF(r,g,b,a);
    palette->setColor(
        static_cast<ColorContext::ColorSet>(set), static_cast<ColorContext::ColorSection>(section),
        static_cast<ColorContext::ColorName>(name), static_cast<ColorContext::ColorState>(state),
        color);
}

}

//ExternalPaletteImpl

ExternalPaletteImpl::ExternalPaletteImpl(MainCtx* ctx, SystemPalette& palette, QObject* parent)
    : QObject(parent)
    , m_palette(palette)
    , m_ctx(ctx)
{
}

ExternalPaletteImpl::~ExternalPaletteImpl()
{
    if (m_provider)
    {
        if (m_provider->close)
            m_provider->close(m_provider);
        if (m_module)
            module_unneed(m_provider, m_module);
        vlc_object_delete(m_provider);
    }
}

bool ExternalPaletteImpl::init()
{
    QString preferedProvider;
#ifndef _WIN32
    if (isGTKBasedEnvironment())
        preferedProvider = "qt-themeprovider-gtk";
#endif

    m_provider = vlc_object_create<vlc_qt_theme_provider_t>(m_ctx->getIntf());
    if (!m_provider)
        return false;


    m_provider->paletteUpdated = PaletteChangedCallback;
    m_provider->paletteUpdatedData = this;

    m_provider->metricsUpdated = MetricsChangedCallback;
    m_provider->metricsUpdatedData = this;

    m_provider->setColorF = setColorRBGAFloat;
    m_provider->setColorInt = setColorRBGAInt;
    m_provider->setColorData = &m_palette;

    m_module = module_need(m_provider, "qt theme provider",
                           preferedProvider.isNull() ? nullptr : qtu(preferedProvider),
                           true);
    if (!m_module)
        return false;
    return true;
}

bool ExternalPaletteImpl::isThemeDark() const
{
    if (!m_provider->isThemeDark)
        return false;
    return m_provider->isThemeDark(m_provider);
}

bool ExternalPaletteImpl::hasCSDImages() const
{
    if (!m_provider->supportThemeImage)
        return false;
    return m_provider->supportThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON);
}


QImage ExternalPaletteImpl::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!m_provider->getThemeImage)
        return {};
    vlc_qt_theme_image_setting imageSettings;
    imageSettings.windowScaleFactor = m_ctx->intfMainWindow()->devicePixelRatio();
    imageSettings.userScaleFacor = m_ctx->getIntfUserScaleFactor();
    imageSettings.u.csdButton.buttonType = type;
    imageSettings.u.csdButton.state = state;
    imageSettings.u.csdButton.maximized = maximized;
    imageSettings.u.csdButton.active = active;
    imageSettings.u.csdButton.bannerHeight = bannerHeight;
    picture_t* pic = m_provider->getThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &imageSettings);
    if (!pic)
        return {};

    QImage::Format format = QImage::Format_Invalid;
    switch (pic->format.i_chroma)
    {
    case VLC_CODEC_ARGB:
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    case VLC_CODEC_RGBA:
        format = QImage::Format_RGBA8888_Premultiplied;
        break;
    case VLC_CODEC_RGB24:
        format = QImage::Format_RGB888;
        break;
    default:
        msg_Err(m_ctx->getIntf(), "unexpected image format from theme provider");
        break;
    }

    return QImage(pic->p[0].p_pixels,
            pic->format.i_visible_width, pic->format.i_visible_height, pic->p[0].i_pitch,
            format,
            ReleaseVLCPictureCb, pic
            );
}

CSDMetrics* ExternalPaletteImpl::getCSDMetrics() const
{
    return m_csdMetrics.get();
}

int ExternalPaletteImpl::update()
{
    if (m_provider->updatePalette)
        return m_provider->updatePalette(m_provider);
    return VLC_EGENERIC;
}

void ExternalPaletteImpl::updateMetrics(vlc_qt_theme_image_type type)
{
    if (m_provider->getThemeMetrics)
    {
        if (type == VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
        {
            m_csdMetrics.reset();
            [&](){
                if (!m_provider->getThemeMetrics)
                    return;
                vlc_qt_theme_metrics metrics;
                memset(&metrics, 0, sizeof(metrics));
                bool ret = m_provider->getThemeMetrics(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &metrics);
                if (!ret)
                    return;
                m_csdMetrics = std::make_unique<CSDMetrics>();
                m_csdMetrics->interNavButtonSpacing = metrics.u.csd.interNavButtonSpacing;

                m_csdMetrics->csdFrameMarginLeft = metrics.u.csd.csdFrameMarginLeft;
                m_csdMetrics->csdFrameMarginRight = metrics.u.csd.csdFrameMarginRight;
                m_csdMetrics->csdFrameMarginTop = metrics.u.csd.csdFrameMarginTop;
                m_csdMetrics->csdFrameMarginBottom = metrics.u.csd.csdFrameMarginBottom;
            }();
            emit CSDMetricsChanged();
        }
    }
}

SystemPalette::SystemPalette(QObject* parent)
    : QObject(parent)
{

}

void SystemPalette::componentComplete()
{
    updatePalette();
}

void SystemPalette::updateAccent(bool dark)
{
    // Exact rounded sRGB Figma tokens. See docs/THEME-MAP.md for the
    // mode-specific HSV relationships used for non-default accents.
    const QColor main(dark ? "#d37918" : "#e8851a");
    QColor custom;
    if (m_ctx)
    {
        char* value = var_InheritString(m_ctx->getIntf(), "qt-accent-color");
        const QString input = QString::fromUtf8(value ? value : "");
        free(value);
        if (input.size() == 7 && input.startsWith(QLatin1Char('#')))
        {
            bool valid = false;
            input.mid(1).toUInt(&valid, 16);
            if (valid)
                custom = QColor(input);
        }
    }
    // The orange onboarding swatch selects the authored orange for each mode.
    if (custom == QColor("#d37918"))
        custom = QColor();

    const auto family = [&](const char* darkValue, const char* lightValue) {
        const QColor authored(dark ? darkValue : lightValue);
        if (!custom.isValid())
            return authored;
        qreal hue = custom.hsvHueF();
        if (hue < 0) // achromatic input must not inherit the orange hue
            hue = 0;
        hue += authored.hsvHueF() - main.hsvHueF();
        hue -= std::floor(hue);
        const qreal saturation = std::clamp<qreal>(custom.hsvSaturationF()
            * authored.hsvSaturationF() / main.hsvSaturationF(), qreal(0), qreal(1));
        const qreal value = std::clamp<qreal>(custom.valueF()
            * authored.valueF() / main.valueF(), qreal(0), qreal(1));
        const QColor result = QColor::fromHsvF(hue, saturation, value);
        return QColor(result.red(), result.green(), result.blue());
    };
    accentMain = custom.isValid() ? custom : main;
    m_playAccent = custom.isValid() ? custom : QColor("#d37918");
    accentBright = family("#fc9729", "#ffa13b");
    accentDim = family("#ab6314", "#965916");
    bgAccentDark = family("#1a1611", "#c6ab89");
    bgAccentBright = family("#371f06", "#d8ba9a");
    accentLogo = family("#d37918", "#c2711a");
    orange500 = accentMain;
    orange800 = accentDim;
    orange400 = accentBright;
    orange200 = accentDim;
}

ColorSchemeModel::ColorScheme SystemPalette::source() const
{
    return m_source;
}

QImage SystemPalette::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!hasCSDImage())
        return QImage();
    assert(m_palettePriv);
    return m_palettePriv->getCSDImage(type, state, maximized, active, bannerHeight);

}

CSDMetrics* SystemPalette::getCSDMetrics() const
{
    if (!m_palettePriv)
        return nullptr;
    return m_palettePriv->getCSDMetrics();
}

bool SystemPalette::hasCSDImage() const
{
    if (!m_palettePriv)
        return false;
    return m_palettePriv->hasCSDImages();
}

void SystemPalette::setSource(ColorSchemeModel::ColorScheme source)
{
    if (m_source == source)
        return;
    m_source = source;

    updatePalette();
    emit sourceChanged();
}

void SystemPalette::setCtx(MainCtx* ctx)
{
    if (ctx == m_ctx)
        return;
    m_ctx = ctx;
    emit ctxChanged();
    updatePalette();
}

void SystemPalette::updatePalette()
{
    m_palettePriv.reset();
    switch(m_source)
    {
    case ColorSchemeModel::System:
        makeSystemPalette();
        break;
    case ColorSchemeModel::Day:
        makeLightPalette();
        break;
    case ColorSchemeModel::Night:
        makeDarkPalette();
        break;
    default:
        break;
    }

    if (m_palettePriv)
    {
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::paletteChanged,
            this, &SystemPalette::updatePalette);
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::CSDMetricsChanged,
            this, &SystemPalette::CSDMetricsChanged);
    }
    emit paletteChanged();

    bool hasCSDImage = m_palettePriv && m_palettePriv->hasCSDImages();
    if (m_hasCSDImage != hasCSDImage)
    {
        m_hasCSDImage = hasCSDImage;
        emit hasCSDImageChanged();
    }
}

static quint64 makeKey(ColorContext::ColorSet colorSet, ColorContext::ColorSection section,
                       ColorContext::ColorName name, ColorContext::ColorState state)
{
    static_assert(VQTC_STATE_COUNT < (1<<4), "");
    static_assert(VQTC_SECTION_COUNT < (1<<4), "");
    static_assert(VQTC_NAME_COUNT < (1<<8), "");
    static_assert(VQTC_SET_COUNT < (1<<16), "");
    return  (colorSet << 16)
        + (name << 8)
        + (section << 4)
        + state;
}

void SystemPalette::setColor(ColorContext::ColorSet colorSet,  ColorContext::ColorSection section,
                             ColorContext::ColorName name, ColorContext::ColorState state, QColor color)
{

    quint64 key = makeKey(colorSet, section, name, state);
    m_colorMap[key] = color;
}


QColor SystemPalette::getColor(ColorContext::ColorSet colorSet, ColorContext::ColorSection section,
                               ColorContext::ColorName name, ColorContext::ColorState state) const
{
    typedef ColorContext C;

    quint64 key = makeKey(colorSet, section, name, state);
    auto it = m_colorMap.find(key);
    if (it != m_colorMap.cend())
        return *it;
    //we don't have the role explicitly set, fallback to the normal state
    key = makeKey(colorSet, section, name, C::Normal);
    it = m_colorMap.find(key);
    if (it != m_colorMap.cend())
        return *it;
    //we don't have the role explicitly set, fallback to the colorSet View
    //TODO do we want finer hierarchy?
    if (colorSet != C::View)
    {
        return getColor(C::View, section, name, state);
    }
    else
    {
        //nothing matches, that's probably an issue, return an ugly color
        return Qt::magenta;
    }
}


void SystemPalette::makeLightPalette()
{
    m_isDark = false;

    updateAccent(false);

    typedef ColorContext C;

    m_colorMap.clear();

    //base set
    {
        C::ColorSet CS = C::View;
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#e7e7e7")); // Light/BG/B1
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#dedede")); // Light/BG/B2

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3

        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // Light/FG/A3

        setColor(CS, C::Bg, C::Negative, C::Normal, QColor("#dccdca")); // D-status-Light-Bg-Negative
        setColor(CS, C::Fg, C::Negative, C::Normal, QColor("#9e3823")); // D-status-Light-Fg-Negative

        setColor(CS, C::Bg, C::Neutral, C::Normal, bgAccentDark); // Light/BG/Accent Dark
        setColor(CS, C::Fg, C::Neutral, C::Normal, accentMain); // Light/Accent/Main

        setColor(CS, C::Bg, C::Positive, C::Normal, QColor("#c7d5ca")); // D-status-Light-Bg-Positive
        setColor(CS, C::Fg, C::Positive, C::Normal, QColor("#127124")); // D-status-Light-Fg-Positive

        setColor(CS, C::Decoration, C::VisualFocus, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::VisualFocus, C::Focused, accentMain); // Light/Accent/Main

        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#acacac")); // Light/Divider/Dim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Light/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#9b9b9b")); // Light/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0

        setColor(CS, C::Decoration, C::Separator, C::Normal, QColor("#acacac")); // Light/Divider/Dim

        setColor(CS, C::Decoration, C::Shadow, C::Normal, setColorAlpha(Qt::black, 0.46)); // D-shadow

        setColor(CS, C::Decoration, C::Accent, C::Normal, accentMain); // Light/Accent/Main
        setColor(CS, C::Fg, C::Link, C::Normal, accentMain); // Light/Accent/Main
    }

    //window banner & miniplayer
    {
        C::ColorSet CS = C::Window;
        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#ababaa"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#d5d5d5")); // Light/BG/A3
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#d5d5d5")); // Light/BG/A3
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#acacac")); // Light/Divider/Dim
    }

    //badges
    {
        C::ColorSet CS = C::Badge;
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#e7e7e7")); // Light/BG/B1
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
    }

    //tab button
    {
        C::ColorSet CS = C::TabButton;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#dddddd")); // Light/BG/A2

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#161616")); // Light/FG/B1
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 804:1208 checked segment
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#d5d5d5")); // D-control-pressed
    }

    //tool button
    {
        C::ColorSet CS = C::ToolButton;
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#8f8f8f")); // 2425:13645 transport/effects
        setColor(CS, C::Fg, C::Highlight, C::Hovered, QColor("#2e2e2e"));
        setColor(CS, C::Fg, C::Highlight, C::Focused, QColor("#2e2e2e"));
        setColor(CS, C::Fg, C::Highlight, C::Pressed, QColor("#161616"));
        setColor(CS, C::Fg, C::Highlight, C::Disabled, QColor("#b3b3b3"));
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#d5d5d5")); // Light/BG/A3

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#161616")); // Light/FG/B1

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Hovered, setColorAlpha(Qt::black, 0.06)); // 177:99 white 6%; Light analogue / D-control-pressed
        setColor(CS, C::Bg, C::Primary, C::Pressed, setColorAlpha(Qt::black, 0.12)); // 177:99 white 6%; Light analogue / D-control-pressed
    }

    //menubar
    {
        C::ColorSet CS = C::MenuBar;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3
    }

    //Item
    {
        C::ColorSet CS = C::Item;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#dddddd")); // Light/BG/A2

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#d5d5d5")); // Light/BG/A3; D-control-selection
        setColor(CS, C::Bg, C::Highlight, C::Focused, QColor("#d5d5d5")); // Light/BG/A3; D-control-selection
        setColor(CS, C::Bg, C::Highlight, C::Hovered, QColor("#d5d5d5")); // Light/BG/A3; D-control-selection
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#161616")); // Light/FG/B1; 3021:9838

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // Light/FG/A3
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled

        setColor(CS, C::Decoration, C::Indicator, C::Normal, accentLogo); // Light/Accent/Logo

        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#d5d5d5")); // D-control-pressed
    }

    //Accent Buttons
    {
        C::ColorSet CS = C::ButtonAccent;
        setColor(CS, C::Bg, C::Highlight, C::Normal, m_playAccent);
        setColor(CS, C::Bg, C::Highlight, C::Hovered, accentBright);
        setColor(CS, C::Bg, C::Highlight, C::Pressed, accentDim);
        setColor(CS, C::Bg, C::Highlight, C::Disabled, QColor("#b3b3b3"));
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#6b6b6b")); // 848:1713 pause pill
        setColor(CS, C::Bg, C::Primary, C::Normal, accentMain); // Light/Accent/Main
        setColor(CS, C::Bg, C::Primary, C::Hovered, accentBright); // Light/Accent/Bright
        setColor(CS, C::Bg, C::Primary, C::Pressed, accentDim); // Light/Accent/DIm
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#b3b3b3")); // Light/BG/A4

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#161616")); // Light/FG/B1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
    }

    //Standard Buttons
    {
        C::ColorSet CS = C::ButtonStandard;
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#161616"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#dedede")); // 500:3063 / D-control-states

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#6b6b6b")); // 500:3061 normal label
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Pressed, QColor("#2e2e2e")); // D-control-pressed: hover foreground
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#2e2e2e")); // 500:3065 hover label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // Light/FG/A3

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#b3b3b3")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#dddddd")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#dedede")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#dedede")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 804:1208 checked segment
    }

    //tooltip
    {
        C::ColorSet CS = C::Tooltip;
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#d5d5d5")); // volume tooltip light analogue
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#2e2e2e"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#b3b3b3")); // D-tooltip; popup panel analogue
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
    }

    //slider
    {
        C::ColorSet CS = C::Slider;
        setColor(CS, C::Bg, C::Neutral, C::Normal, QColor("#888888"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#dedede")); // Light/BG/B2
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 3056:10877 slider fill
        setColor(CS, C::Bg, C::Secondary, C::Disabled, QColor("#b3b3b3")); // Light/BG/A4 disabled fill
        setColor(CS, C::Fg, C::Primary, C::Normal, accentMain); // Light/Accent/Main
        setColor(CS, C::Fg, C::Positive, C::Normal, QColor("#127124")); // D-status-Light-Fg-Positive
        setColor(CS, C::Fg, C::Neutral, C::Normal, accentMain); // Light/Accent/Main
        setColor(CS, C::Fg, C::Negative, C::Normal, QColor("#9e3823")); // D-status-Light-Fg-Negative
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#b3b3b3")); // D-control-disabled
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#dedede")); // D-control-disabled
    }

    //Combo box
    {
        C::ColorSet CS = C::ComboBox;
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // Light/FG/A2
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#b3b3b3")); // Light/BG/A4
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#dedede")); // 2372:8300 2px B2 well
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#dddddd")); // D-control-hover
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#d5d5d5")); // D-control-pressed
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled
    }

    //TextField
    {
        C::ColorSet CS = C::TextField;
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // placeholder
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#b3b3b3")); // 3062:10959 input rim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Light/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#9b9b9b")); // Light/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#dedede")); // 3062:10959 / 3176:16553 input well
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // 3176:16554 input label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled
    }

    //Switch
    {
        C::ColorSet CS = C::Switch;
        setColor(CS, C::Fg, C::Link, C::Normal, QColor("#3b3b3b"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#b3b3b3")); // Light/BG/A4
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#6b6b6b")); // Light/FG/A3
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#acacac")); // Light/Divider/Dim

        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // Light/BG/Accent Bright
        setColor(CS, C::Fg, C::Secondary, C::Normal, accentDim); // Light/Accent/DIm
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#b3b3b3")); // D-control-disabled
        setColor(CS, C::Bg, C::Secondary, C::Disabled, QColor("#b3b3b3")); // D-control-disabled
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled
        setColor(CS, C::Fg, C::Secondary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled
        setColor(CS, C::Fg, C::Secondary, C::Hovered, accentMain); // D-control-hover
        setColor(CS, C::Fg, C::Secondary, C::Pressed, accentBright); // D-control-pressed
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#6b6b6b")); // D-control-hover
        setColor(CS, C::Fg, C::Primary, C::Pressed, QColor("#2e2e2e")); // D-control-pressed
    }

    //SpinBox
    {
        C::ColorSet CS = C::SpinBox;
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#b3b3b3")); // 3062:10959 input rim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Light/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#9b9b9b")); // Light/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#d5d5d5"), 0.0)); // Light/BG/A3 × alpha 0

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#dddddd")); // Light/BG/A2
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#3c3c3c")); // Light/FG/A1
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#dedede")); // 3062:10959 / 3176:16553 input well
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#2e2e2e")); // 3176:16554 input label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#6b6b6b")); // D-control-disabled
    }
}

void SystemPalette::makeDarkPalette()
{
    m_isDark = true;

    updateAccent(true);

    m_colorMap.clear();

    typedef ColorContext C;

    {
        C::ColorSet CS = C::View;
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#0e0e0e")); // Dark/BG/B1
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#090909")); // Dark/BG/B2

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4

        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // Dark/FG/A3

        setColor(CS, C::Bg, C::Negative, C::Normal, QColor("#2e1813")); // D-status-Dark-Bg-Negative
        setColor(CS, C::Fg, C::Negative, C::Normal, QColor("#e15032")); // D-status-Dark-Fg-Negative

        setColor(CS, C::Bg, C::Neutral, C::Normal, bgAccentDark); // Dark/BG/Accent Dark
        setColor(CS, C::Fg, C::Neutral, C::Normal, accentMain); // Dark/Accent/Main

        setColor(CS, C::Bg, C::Positive, C::Normal, QColor("#102414")); // D-status-Dark-Bg-Positive
        setColor(CS, C::Fg, C::Positive, C::Normal, QColor("#19a234")); // D-status-Dark-Fg-Positive

        setColor(CS, C::Decoration, C::VisualFocus, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::VisualFocus, C::Focused, accentMain); // Dark/Accent/Main

        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#222222")); // Dark/Divider/Dim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Dark/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#373737")); // Dark/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0

        setColor(CS, C::Decoration, C::Shadow, C::Normal, setColorAlpha(Qt::black, 0.46)); // D-shadow

        setColor(CS, C::Decoration, C::Separator, C::Normal, QColor("#222222")); // Dark/Divider/Dim

        setColor(CS, C::Decoration, C::Accent, C::Normal, accentMain); // Dark/Accent/Main
        setColor(CS, C::Fg, C::Link, C::Normal, accentMain); // Dark/Accent/Main
    }

    //window banner & miniplayer
    {
        C::ColorSet CS = C::Window;
        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#1b1b1b"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#1b1b1b")); // Dark/BG/A3
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#1b1b1b")); // Dark/BG/A3
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#222222")); // Dark/Divider/Dim
    }

    //badges
    {
        C::ColorSet CS = C::Badge;
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#0e0e0e")); // Dark/BG/B1
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#969696")); // Dark/FG/A1
    }

    //tab button
    {
        C::ColorSet CS = C::TabButton;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#1f1f1f")); // Dark/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#1f1f1f")); // Dark/BG/A2

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // Dark/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#c6c6c6")); // Dark/FG/2
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 804:1208 checked segment
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#222222")); // D-control-pressed
    }

    //tool button
    {
        C::ColorSet CS = C::ToolButton;
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#8f8f8f")); // 187:328 transport/effects
        setColor(CS, C::Fg, C::Highlight, C::Hovered, QColor("#c6c6c6"));
        setColor(CS, C::Fg, C::Highlight, C::Focused, QColor("#c6c6c6"));
        setColor(CS, C::Fg, C::Highlight, C::Pressed, QColor("#eaeaea"));
        setColor(CS, C::Fg, C::Highlight, C::Disabled, QColor("#404040"));
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#1b1b1b")); // Dark/BG/A3

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // Dark/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#c6c6c6")); // Dark/FG/2

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Hovered, setColorAlpha(Qt::white, 0.06)); // 177:99 white 6%; Light analogue / D-control-pressed
        setColor(CS, C::Bg, C::Primary, C::Pressed, setColorAlpha(Qt::white, 0.12)); // 177:99 white 6%; Light analogue / D-control-pressed
    }

    //menubar
    {
        C::ColorSet CS = C::MenuBar;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#1f1f1f")); // Dark/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#1f1f1f")); // Dark/BG/A2
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // Dark/FG/A2
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4
    }

    //Item
    {
        C::ColorSet CS = C::Item;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#1f1f1f")); // Dark/BG/A2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#1f1f1f")); // Dark/BG/A2

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#222222")); // Dark/BG/A1; D-control-selection
        setColor(CS, C::Bg, C::Highlight, C::Focused, QColor("#222222")); // Dark/BG/A1; D-control-selection
        setColor(CS, C::Bg, C::Highlight, C::Hovered, QColor("#222222")); // Dark/BG/A1; D-control-selection
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#eaeaea")); // Dark/FG/1; 3021:9838

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // Dark/FG/A3
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // D-control-disabled

        setColor(CS, C::Decoration, C::Indicator, C::Normal, accentLogo); // Dark/Accent/Logo
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#222222")); // D-control-pressed
    }

    //Accent Buttons
    {
        C::ColorSet CS = C::ButtonAccent;
        setColor(CS, C::Bg, C::Highlight, C::Normal, m_playAccent);
        setColor(CS, C::Bg, C::Highlight, C::Hovered, accentBright);
        setColor(CS, C::Bg, C::Highlight, C::Pressed, accentDim);
        setColor(CS, C::Bg, C::Highlight, C::Disabled, QColor("#191919"));
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#6b6b6b")); // 848:1713 pause pill
        setColor(CS, C::Bg, C::Primary, C::Normal, accentMain); // Dark/Accent/Main
        setColor(CS, C::Bg, C::Primary, C::Hovered, accentBright); // Dark/Accent/Bright
        setColor(CS, C::Bg, C::Primary, C::Pressed, accentDim); // Dark/Accent/DIm
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#191919")); // Dark/BG/A4

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#161616")); // Dark/FG/B1
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
    }

    //Standard Buttons
    {
        C::ColorSet CS = C::ButtonStandard;
        setColor(CS, C::Fg, C::Highlight, C::Normal, accentMain);
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#090909")); // 500:3063 / D-control-states

        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#6b6b6b")); // 500:3061 normal label
        setColor(CS, C::Fg, C::Primary, C::Focused, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Fg, C::Primary, C::Pressed, QColor("#8f8f8f")); // D-control-pressed: hover foreground
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#8f8f8f")); // 500:3065 hover label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4

        setColor(CS, C::Decoration, C::Border, C::Normal, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus; accessible keyboard outline
        setColor(CS, C::Decoration, C::Border, C::Hovered, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#191919")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#1f1f1f")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#090909")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#090909")); // 500:3063 / D-control-states
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 804:1208 checked segment
    }

    //tooltip
    {
        C::ColorSet CS = C::Tooltip;
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#0f0f0f")); // 2245:9805 numeric volume tooltip
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#969696"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#191919")); // D-tooltip; popup panel analogue
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#969696")); // Dark/FG/A1
    }

    //slider
    {
        C::ColorSet CS = C::Slider;
        setColor(CS, C::Bg, C::Neutral, C::Normal, QColor("#090909"));
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#090909")); // Dark/BG/B2
        setColor(CS, C::Bg, C::Primary, C::Focused, QColor("#090909")); // Dark/BG/B2
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#090909")); // Dark/BG/B2

        setColor(CS, C::Fg, C::Primary, C::Normal, accentMain); // Dark/Accent/Main
        setColor(CS, C::Fg, C::Positive, C::Normal, QColor("#19a234")); // D-status-Dark-Fg-Positive
        setColor(CS, C::Fg, C::Neutral, C::Normal, accentMain); // Dark/Accent/Main
        setColor(CS, C::Fg, C::Negative, C::Normal, QColor("#e15032")); // D-status-Dark-Fg-Negative
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // 3056:10877 slider fill
        setColor(CS, C::Bg, C::Secondary, C::Disabled, QColor("#151515")); // Dark/BG/A5 disabled fill
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // Dark/FG/A4 disabled thumb
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#191919")); // D-control-disabled
    }

    //Combo box
    {
        C::ColorSet CS = C::ComboBox;
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // Dark/FG/A2
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#191919")); // Dark/BG/A4
        setColor(CS, C::Bg, C::Secondary, C::Normal, QColor("#1f1f1f")); // Dark/BG/A2
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#090909")); // 2372:8300 2px B2 well
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // D-control-focus
        setColor(CS, C::Bg, C::Primary, C::Hovered, QColor("#1f1f1f")); // D-control-hover
        setColor(CS, C::Bg, C::Primary, C::Pressed, QColor("#222222")); // D-control-pressed
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // D-control-disabled
    }

    //TextField
    {
        C::ColorSet CS = C::TextField;
        setColor(CS, C::Fg, C::Secondary, C::Normal, QColor("#6b6b6b")); // placeholder
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#191919")); // 3062:10959 input rim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Dark/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#373737")); // Dark/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#222222")); // Dark/BG/A1
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#090909")); // 3062:10959 / 3176:16553 input well
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // 3176:16554 input label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // D-control-disabled
    }

    //Switch
    {
        C::ColorSet CS = C::Switch;
        setColor(CS, C::Fg, C::Link, C::Normal, accentMain);
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#191919")); // Dark/BG/A4
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#404040")); // Dark/FG/A4
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#222222")); // Dark/Divider/Dim
        setColor(CS, C::Bg, C::Secondary, C::Normal, bgAccentBright); // Dark/BG/Accent Bright
        setColor(CS, C::Fg, C::Secondary, C::Normal, accentDim); // Dark/Accent/DIm
        setColor(CS, C::Bg, C::Primary, C::Disabled, QColor("#191919")); // D-control-disabled
        setColor(CS, C::Bg, C::Secondary, C::Disabled, QColor("#191919")); // D-control-disabled
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // D-control-disabled
        setColor(CS, C::Fg, C::Secondary, C::Disabled, QColor("#404040")); // D-control-disabled
        setColor(CS, C::Fg, C::Secondary, C::Hovered, accentMain); // D-control-hover
        setColor(CS, C::Fg, C::Secondary, C::Pressed, accentBright); // D-control-pressed
        setColor(CS, C::Fg, C::Primary, C::Hovered, QColor("#6b6b6b")); // D-control-hover
        setColor(CS, C::Fg, C::Primary, C::Pressed, QColor("#8f8f8f")); // D-control-pressed
    }

    //Spinbox
    {
        C::ColorSet CS = C::SpinBox;
        setColor(CS, C::Decoration, C::Border, C::Normal, QColor("#191919")); // 3062:10959 input rim
        setColor(CS, C::Decoration, C::Border, C::Focused, accentMain); // Dark/Accent/Main
        setColor(CS, C::Decoration, C::Border, C::Hovered, QColor("#373737")); // Dark/Divider/Bright
        setColor(CS, C::Decoration, C::Border, C::Disabled, setColorAlpha(QColor("#1b1b1b"), 0.0)); // Dark/BG/A3 × alpha 0

        setColor(CS, C::Bg, C::Highlight, C::Normal, QColor("#222222")); // Dark/BG/A1
        setColor(CS, C::Fg, C::Highlight, C::Normal, QColor("#969696")); // Dark/FG/A1
        setColor(CS, C::Bg, C::Primary, C::Normal, QColor("#090909")); // 3062:10959 / 3176:16553 input well
        setColor(CS, C::Fg, C::Primary, C::Normal, QColor("#8f8f8f")); // 3176:16554 input label
        setColor(CS, C::Fg, C::Primary, C::Disabled, QColor("#404040")); // D-control-disabled
    }
}

void SystemPalette::makeSystemPalette()
{
    const auto fallback = [this]() {
        const QPalette palette = qApp->palette();
        if (palette.color(QPalette::Normal, QPalette::Base).lightness()
            < palette.color(QPalette::Normal, QPalette::Text).lightness())
            makeDarkPalette();
        else
            makeLightPalette();
    };
    if (!m_ctx)
    {
        // No provider: use Qt's OS palette only to detect darkness.
        fallback();
        return;
    }

    auto palette = std::make_unique<ExternalPaletteImpl>(m_ctx, *this);
    if (!palette->init())
    {
        // No provider: use Qt's OS palette only to detect darkness.
        fallback();
        return;
    }

    // Providers still observe OS theme changes and supply window chrome, but
    // their semantic colors (including stock accents) must never replace ours.
    if (palette->isThemeDark())
        makeDarkPalette();
    else
        makeLightPalette();
    palette->updateMetrics(VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON);
    m_palettePriv = std::move(palette);
}
