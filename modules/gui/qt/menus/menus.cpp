/*****************************************************************************
 * menus.cpp : Qt menus
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/** \todo
 * - Remove static currentGroup
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_intf_strings.h>
#include <vlc_aout.h>                             /* audio_output_t */
#include <vlc_interface.h>

#include "menus.hpp"

#include "maininterface/mainctx.hpp"                     /* View modifications */
#include "maininterface/mainctx_submodels.hpp"
#include "dialogs/dialogs_provider.hpp"                   /* Dialogs display */
#include "dialogs/help/help.hpp"
#include "player/player_controller.hpp"                      /* Input Management */
#include "playlist/playlist_controller.hpp"
#include "dialogs/extensions/extensions_manager.hpp"                 /* Extensions menu */
#include "dialogs/extended/extended_panels.hpp"
#include "dialogs/systray/systray.hpp"
#include "util/varchoicemodel.hpp"
#include "util/color_scheme_model.hpp"
#include "util/colorizedsvgicon.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlrecentmediamodel.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"


#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QSignalMapper>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QPainter>
#include <QPainterPath>
#include <vlc_actions.h>
#include "style/systempalette.hpp"
#include "util/vlchotkeyconverter.hpp"

namespace {
// Keep QMenu's action/model/keyboard APIs, but draw these two popups with the
// authored menu metrics. The optional traditional/native menubar is untouched.
class PlayerMenuStyle final : public QProxyStyle
{
public:
    PlayerMenuStyle(qt_intf_t* intf, QMenu* menu)
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
        , m_scale(intf->p_mi->getIntfScaleFactor())
    {
        setParent(menu);
        m_palette.setCtx(intf->p_mi);
        m_palette.setSource(intf->p_mi->getColorScheme()->currentScheme());
        connect(intf->p_mi->getColorScheme(), &ColorSchemeModel::currentChanged,
                this, [this, ctx = intf->p_mi]() {
            m_palette.setSource(ctx->getColorScheme()->currentScheme());
        });
        connect(&m_palette, &SystemPalette::paletteChanged, menu, [this, menu]() {
            applyPalette(menu);
            menu->update();
        });
    }

    int px(int value) const { return qRound(value * m_scale); }
    bool dark() const { return m_palette.property("isDark").toBool(); }
    QColor foreground() const { return dark() ? QColor("#8f8f8f") : QColor("#2e2e2e"); }
    QColor quiet() const { return QColor("#6b6b6b"); }
    QColor accent() const { return dark() ? m_palette.get_accentMain() : m_palette.get_accentDim(); }

    void applyPalette(QMenu* menu) const
    {
        QPalette palette = menu->palette();
        palette.setColor(QPalette::Text, foreground());
        palette.setColor(QPalette::WindowText, foreground());
        palette.setColor(QPalette::Window, dark() ? QColor("#191919") : QColor("#d5d5d5"));
        menu->setPalette(palette);
    }

    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override
    {
        switch (metric) {
        case PM_MenuPanelWidth: return 0;
        case PM_MenuHMargin: case PM_MenuVMargin: return px(4);
        case PM_SmallIconSize: return px(24);
        default: return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* data) const override
    {
        if (hint == SH_Menu_SubMenuPopupDelay) return 150;
        if (hint == SH_Menu_Mask && widget) {
            if (auto mask = qstyleoption_cast<QStyleHintReturnMask*>(data)) {
                // A shape mask also clips corners on X11 without a compositing
                // manager; alpha-only QWidget corners otherwise become black.
                QPainterPath path;
                path.addRoundedRect(widget->rect(), px(10), px(10));
                mask->region = QRegion(path.toFillPolygon().toPolygon());
                return 1;
            }
        }
        return QProxyStyle::styleHint(hint, option, widget, data);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& size, const QWidget* widget) const override
    {
        if (type != CT_MenuItem)
            return QProxyStyle::sizeFromContents(type, option, size, widget);
        const auto item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if (!item) return size;
        if (item->menuItemType == QStyleOptionMenuItem::Separator) return QSize(px(1), px(1));
        const auto parts = item->text.split('\t');
        const int shortcut = parts.size() > 1 ? item->fontMetrics.horizontalAdvance(parts[1]) + px(24) : 0;
        const int width = widget ? widget->property("playerMenuWidth").toInt() : 265;
        // QMenu adds the widest shortcut column AFTER sizeFromContents().
        // Account for that once, including display-only engine bindings.
        int shortcutColumn = 0;
        if (auto menu = qobject_cast<const QMenu*>(widget)) {
            for (auto action : menu->actions()) {
                QString label = action->text().section('\t', 1, 1);
                if (label.isEmpty()) label = action->shortcut().toString(QKeySequence::NativeText);
                shortcutColumn = qMax(shortcutColumn, item->fontMetrics.horizontalAdvance(label));
            }
        }
        return QSize(qMax(px(width - 8), item->fontMetrics.horizontalAdvance(parts[0]) + shortcut + px(82)) - shortcutColumn, px(48));
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget) const override
    {
        if (element != PE_PanelMenu) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        // Opaque fallback: QWidget popups cannot sample the native video child.
        painter->setBrush(dark() ? QColor("#191919") : QColor("#d5d5d5"));
        painter->drawRoundedRect(option->rect, px(10), px(10));
        painter->restore();
    }

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const override
    {
        const auto item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if (element != CE_MenuItem || !item) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect row = item->rect;
        if (item->menuItemType == QStyleOptionMenuItem::Separator) {
            painter->fillRect(row, dark() ? QColor("#222222") : QColor("#acacac"));
            painter->restore();
            return;
        }
        const bool enabled = item->state & State_Enabled;
        if (item->state & State_Selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(dark() ? QColor("#1f1f1f") : QColor("#dddddd"));
            painter->drawRoundedRect(row, px(6), px(6));
        }
        painter->setFont(item->font);
        painter->setPen(enabled ? foreground() : (dark() ? QColor("#404040") : quiet()));
        const auto parts = item->text.split('\t');
        QRect textRect = row.adjusted(px(48), 0, -px(14), 0);
        const int trailing = parts.size() > 1 ? item->fontMetrics.horizontalAdvance(parts[1]) + px(24)
                                             : (item->menuItemType == QStyleOptionMenuItem::SubMenu ? px(24) : 0);
        painter->drawText(textRect.adjusted(0, 0, -trailing, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextHideMnemonic,
                          item->fontMetrics.elidedText(parts[0], Qt::ElideRight, textRect.width() - trailing));
        painter->setPen(quiet());
        if (parts.size() > 1)
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignRight, parts[1]);
        if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
            const QPointF center(row.right() - px(22), row.center().y());
            painter->setPen(QPen(enabled ? foreground() : quiet(), 1.5 * m_scale));
            painter->drawLine(center + QPointF(-px(2), -px(5)), center + QPointF(px(3), 0));
            painter->drawLine(center + QPointF(px(3), 0), center + QPointF(-px(2), px(5)));
        }
        const QRect iconRect(row.left() + px(14), row.top() + px(12), px(24), px(24));
        if (item->checked) {
            painter->setPen(QPen(accent(), 2 * m_scale));
            painter->drawLine(iconRect.topLeft() + QPoint(px(5), px(12)), iconRect.topLeft() + QPoint(px(10), px(17)));
            painter->drawLine(iconRect.topLeft() + QPoint(px(10), px(17)), iconRect.topLeft() + QPoint(px(20), px(7)));
        } else if (!item->icon.isNull()) {
            item->icon.paint(painter, iconRect, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
        }
        painter->restore();
    }
private:
    SystemPalette m_palette;
    qreal m_scale;
};

void menuIcon(QAction* action, const char* name)
{
    const QString path = QStringLiteral(":/menu/redesign/") + QString::fromLatin1(name) + QStringLiteral(".svg");
    action->setProperty("playerMenuIcon", path);
}

// Display only: the engine already handles these bindings. Do not register a
// second QAction shortcut, which would steal/duplicate the engine hotkey.
void configuredShortcut(qt_intf_t* intf, QAction* action, const char* key)
{
    char* value = var_InheritString(intf, key);
    const QString binding = QString::fromUtf8(value ? value : "").section('\t', 0, 0);
    free(value);
    if (binding.isEmpty()) return;
    const auto code = vlc_str2keycode(qtu(binding));
    if (!code) return;
    QString label = VLCKeyToString(code, true);
#ifndef __APPLE__
    // NativeText supplies platform modifier spelling; VLC's locale conversion
    // also covers media/mouse keys that QKeySequence cannot represent.
    const auto sequence = QKeySequence::fromString(VLCKeyToString(code, false), QKeySequence::PortableText);
    if (!sequence.isEmpty()) label = sequence.toString(QKeySequence::NativeText);
#endif
    action->setText(action->text() + '\t' + label);
}
}

bool VLCMenuBar::hasEngineHotkey(qt_intf_t* intf, uint_fast32_t key)
{
    for (auto name = vlc_actions_get_key_names(intf); *name; ++name) {
        uint_fast32_t* keys = nullptr;
        const size_t count = vlc_actions_get_keycodes(intf, *name, false, &keys);
        bool found = false;
        for (size_t i = 0; i < count; ++i) found |= keys[i] == key;
        free(keys);
        if (found) return true;
    }
    return false;
}

void VLCMenuBar::styleMenu(qt_intf_t* intf, QMenu* menu, int width)
{
    if (!intf->p_mi) return;
    auto style = dynamic_cast<PlayerMenuStyle*>(menu->style());
    if (!style) {
        style = new PlayerMenuStyle(intf, menu);
        menu->setStyle(style);
        menu->setAttribute(Qt::WA_TranslucentBackground);
        menu->setWindowFlag(Qt::FramelessWindowHint);
        connect(menu, &QMenu::aboutToShow, menu, [intf, menu, width]() {
            styleMenu(intf, menu, width);
        });
    }
    menu->setProperty("playerMenuWidth", width);
    style->applyPalette(menu);
    QFont font(QStringLiteral("Inter"));
    font.setPixelSize(style->px(14));
    menu->setFont(font);
    for (auto action : menu->actions()) {
        if (action->shortcut() == QKeySequence(QStringLiteral("Shift+F1")) &&
            hasEngineHotkey(intf, KEY_MODIFIER_SHIFT | KEY_F1))
            action->setShortcut(QKeySequence());
        const auto icon = action->property("playerMenuIcon").toString();
        if (!icon.isEmpty())
            action->setIcon(ColorizedSvgIcon(icon, style->foreground(), style->quiet(), style->accent()));
#ifdef Q_OS_MACOS
        // QMacStyle defaults to suppressing menu icons and shortcut columns.
        // These are authored QWidget popups (including QML-triggered menus),
        // not the native macOS menu bar. Opt in per action, not globally.
        action->setIconVisibleInMenu(true);
        if (!action->shortcut().isEmpty() && !action->text().contains('\t'))
            action->setText(action->text() + '\t' + action->shortcut().toString(QKeySequence::NativeText));
#endif
        if (auto submenu = action->menu())
            styleMenu(intf, submenu, 265);
    }
}

using namespace vlc::playlist;

/*
  This file defines the main menus and the pop-up menu (right-click menu)
  and the systray menu (in that order in the file)

  There are 4 menus that have to be rebuilt every time there are called:
  Audio, Video, Navigation, view
  4 functions are building those menus: AudioMenu, VideoMenu, NavigMenu, View

  */

RendererMenu *VLCMenuBar::rendererMenu = NULL;

/**
 * @brief Add static entries to DP in menus
 **/
template<typename Functor>
QAction *addDPStaticEntry( QMenu *menu,
                       const QString& text,
                       const char *icon,
                       const Functor member,
                       const char *shortcut = NULL,
                       QAction::MenuRole role = QAction::NoRole
                       )
{
    QAction *action = NULL;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        action = menu->addAction( ColorizedSvgIcon::colorizedIconForWidget( icon, menu ), text, THEDP, member );
    }
    else
#endif
    {
        action = menu->addAction( text, THEDP, member );
    }
    if( !EMPTY_STR( shortcut ) )
        action->setShortcut( qfut( shortcut ) );
#ifdef __APPLE__
    action->setMenuRole( role );
#else
    Q_UNUSED( role );
#endif
#ifdef Q_OS_MACOS
    // Keep legacy native-menu policy, but retain the source for authored
    // QWidget popups: styleMenu() can opt those actions into visible icons.
    if (!EMPTY_STR(icon))
        action->setProperty("playerMenuIcon", QString::fromLatin1(icon));
#endif
    return action;
}

/**
 * @brief Add static entries to MIM in menus
 **/
template<typename Fun>
static QAction* addMIMStaticEntry( qt_intf_t *p_intf,
                            QMenu *menu,
                            const QString& text,
                            const char *icon,
                            Fun member )
{
    QAction *action;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        action = menu->addAction( text, THEMIM,  member );
        action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( icon, menu ) );
    }
    else
#endif
    {
        action = menu->addAction( text, THEMIM, member );
    }
#ifdef Q_OS_MACOS
    if (!EMPTY_STR(icon))
        action->setProperty("playerMenuIcon", QString::fromLatin1(icon));
#endif
    return action;
}

template<typename Fun>
static QAction* addMPLStaticEntry( qt_intf_t *p_intf,
                            QMenu *menu,
                            const QString& text,
                            const char *icon,
                            Fun member )
{
    QAction *action;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        action = menu->addAction( text, THEMPL,  member );
        action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( icon, menu ) );
    }
    else
#endif
    {
        action = menu->addAction( text, THEMPL, member );
    }
#ifdef Q_OS_MACOS
    if (!EMPTY_STR(icon))
        action->setProperty("playerMenuIcon", QString::fromLatin1(icon));
#endif
    return action;
}

/*****************************************************************************
 * All normal menus
 * Simple Code
 *****************************************************************************/

/**
 * Main Menu Bar Creation
 **/
VLCMenuBar::VLCMenuBar(QObject* parent)
    : QObject(parent)
{}


/**
 * Media ( File ) Menu
 * Opening, streaming and quit
 **/
void VLCMenuBar::FileMenu(qt_intf_t *p_intf, QMenu *menu)
{
    auto mi = p_intf->p_mi;
    QAction *action;

    //use a lambda here as the Triggrered signal is emiting and it will pass false (checked) as a first argument
    addDPStaticEntry( menu, qtr( "Open &File..." ),
        ":/menu/file.svg", []() { THEDP->simpleOpenDialog(); } , "Ctrl+O" );
    addDPStaticEntry( menu, qtr( "&Open Multiple Files..." ),
        ":/menu/file.svg", &DialogsProvider::openFileDialog, "Ctrl+Shift+O" );
    addDPStaticEntry( menu, qfut( I_OP_OPDIR ),
        ":/menu/folder.svg", &DialogsProvider::PLOpenDir, "Ctrl+F" );
    addDPStaticEntry( menu, qtr( "Open &Disc..." ),
        ":/menu/disc.svg", &DialogsProvider::openDiscDialog, "Ctrl+D" );
    addDPStaticEntry( menu, qtr( "Open &Network Stream..." ),
        ":/menu/network.svg", &DialogsProvider::openNetDialog, "Ctrl+N" );
    addDPStaticEntry( menu, qtr( "Open &Capture Device..." ),
        ":/menu/capture-card.svg", &DialogsProvider::openCaptureDialog, "Ctrl+C" );

    addDPStaticEntry( menu, qtr( "Open &Location from clipboard" ),
                      NULL, &DialogsProvider::openUrlDialog, "Ctrl+V" );

    if( mi && var_InheritBool( p_intf, "save-recentplay" ) && mi->hasMediaLibrary() )
    {
        MLRecentMediaModel* recentModel = new MLRecentMediaModel(nullptr);
        recentModel->setMl(mi->getMediaLibrary());
        recentModel->setLimit(10);
        QMenu* recentsMenu = new RecentMenu(recentModel, mi->getMediaLibrary(), menu);
        recentsMenu->setTitle(qtr( "Open &Recent Media" ) );
        recentModel->setParent(recentsMenu);
        menu->addMenu( recentsMenu );
    }

    menu->addSeparator();

    addDPStaticEntry( menu, qfut( I_PL_SAVE ), "", &DialogsProvider::savePlayingToPlaylist,
        "Ctrl+Y" );

    addDPStaticEntry( menu, qtr( "Conve&rt / Save..." ), "",
        &DialogsProvider::openAndTranscodingDialogs, "Ctrl+R" );
    addDPStaticEntry( menu, qtr( "&Stream..." ),
        ":/menu/stream.svg", &DialogsProvider::openAndStreamingDialogs, "Ctrl+S" );
    menu->addSeparator();

    action = menu->addAction( qtr( "Quit at the end of play queue" ), [playlist = THEMPL](bool checked){
        if (checked)
            playlist->setMediaStopAction(PlaylistController::MEDIA_STOPPED_EXIT);
        else
            playlist->setMediaStopAction(PlaylistController::MEDIA_STOPPED_CONTINUE);
    });
    action->setCheckable( true );
    action->setChecked( THEMPL->getMediaStopAction() ==  PlaylistController::MEDIA_STOPPED_EXIT);

    VLCSystray* systray = mi ? mi->getSysTray() : nullptr;
    if( systray )
    {
        action = menu->addAction( qtr( "Close to systray"), systray,
                                 &VLCSystray::toggleUpdateMenu );
    }

    addDPStaticEntry( menu, qtr( "&Quit" ) ,
        ":/menu/exit.svg", &DialogsProvider::quit, "Ctrl+Q" );
}

void VLCMenuBar::MainMenu(qt_intf_t* p_intf, QMenu* menu)
{
    // Build the original command banks, then move only the authored promotions.
    // QAction ownership stays in the banks, themselves owned by this root.
    auto more = new QMenu(qtr("More"), menu);
    auto media = more->addMenu(qtr("Media"));
    FileMenu(p_intf, media);
    const auto files = media->actions();
    auto promote = [menu](QMenu* bank, QAction* action, const QString& title, const char* icon) {
        bank->removeAction(action);
        action->setText(title);
        menuIcon(action, icon);
        menu->addAction(action);
    };
    promote(media, files[0], qtr("Open File(s)"), "file");
    promote(media, files[2], qtr("Open Folder"), "folder");
    promote(media, files[4], qtr("Open Network Stream"), "network");
    promote(media, files[3], qtr("Open Disc"), "disc");
    promote(media, files[5], qtr("Open Capture Device"), "capture");
    for (auto action : files) {
        if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+R")))
            promote(media, action, qtr("Convert"), "convert");
    }
    for (auto action : files) {
        if (dynamic_cast<RecentMenu*>(action->menu()))
            promote(media, action, qtr("Open Recent"), "recent");
    }
    menu->addSeparator();
    auto profile = menu->addMenu(qtr("Profile"));
    profile->menuAction()->setEnabled(false);
    menuIcon(profile->menuAction(), "profile");
    menuIcon(menu->addAction(qtr("Keyboard Shortcuts"), THEDP, &DialogsProvider::keyboardShortcutsDialog), "hotkeys");
    auto tools = more->addMenu(qtr("Tools"));
    ToolsMenu(p_intf, tools);
    const auto toolActions = tools->actions();
    if (!p_intf->b_isDialogProvider)
        promote(tools, toolActions[toolActions.size() - 2], qtr("Customize Player"), "customize");
    promote(tools, toolActions.last(), qtr("Settings"), "settings");
    auto help = more->addMenu(qtr("Help"));
    HelpMenu(p_intf, help);
    promote(help, help->actions().last(), qtr("About VLC"), "info");
    NavigMenu(p_intf, more->addMenu(qtr("Playback")));
    VideoMenu(p_intf, more->addMenu(qtr("Video")));
    AudioMenu(p_intf, more->addMenu(qtr("Audio")));
    SubtitleMenu(p_intf, more->addMenu(qtr("Subtitle")));
    ViewMenu(p_intf, more->addMenu(qtr("View")));
    menu->addMenu(more);
    styleMenu(p_intf, menu, 375);
}

/**
 * Tools, like Media Information, Preferences or Messages
 **/
void VLCMenuBar::ToolsMenu( qt_intf_t *p_intf, QMenu *menu )
{
    addDPStaticEntry( menu, qtr( "&Effects and Filters"), ":/menu/ic_fluent_options.svg",
            &DialogsProvider::extendedDialog, "Ctrl+E" );

    addDPStaticEntry( menu, qtr( "&Track Synchronization"), ":/menu/ic_fluent_options.svg",
            &DialogsProvider::synchroDialog, "" );

    addDPStaticEntry( menu, qfut( I_MENU_INFO ) , ":/menu/info.svg",
        QOverload<>::of(&DialogsProvider::mediaInfoDialog), "Ctrl+I" );

    addDPStaticEntry( menu, qfut( I_MENU_CODECINFO ) ,
        ":/menu/info.svg", &DialogsProvider::mediaCodecDialog, "Ctrl+J" );

#ifdef ENABLE_VLM
    addDPStaticEntry( menu, qfut( I_MENU_VLM ), "", &DialogsProvider::vlmDialog,
        "Ctrl+Shift+W" );
#endif

    addDPStaticEntry( menu, qtr( "Program Guide" ), "", &DialogsProvider::epgDialog,
        "" );

    addDPStaticEntry( menu, qfut( I_MENU_MSG ),
        ":/menu/messages.svg", &DialogsProvider::messagesDialog, "Ctrl+M" );

    addDPStaticEntry( menu, qtr( "Plu&gins and extensions" ),
        "", &DialogsProvider::pluginDialog );
    menu->addSeparator();

    if( !p_intf->b_isDialogProvider )
        addDPStaticEntry( menu, qtr( "Customi&ze Interface..." ),
            ":/menu/preferences.svg", &DialogsProvider::showToolbarEditorDialog);

    addDPStaticEntry( menu, qtr( "&Preferences" ),
        ":/menu/preferences.svg", &DialogsProvider::prefsDialog, "Ctrl+P", QAction::PreferencesRole );
}

/**
 * View Menu
 * Interface modification, load other interfaces, activate Extensions
 * \param current, set to NULL for menu creation, else for menu update
 **/
void VLCMenuBar::ViewMenu(qt_intf_t *p_intf, QMenu *menu)
{
    QAction *action;

    MainCtx *mi = p_intf->p_mi;
    assert( mi );
    assert(menu);

    //menu->clear();
    //HACK menu->clear() does not delete submenus
    QList<QAction*> actions = menu->actions();
    foreach( QAction *a, actions )
    {
        QMenu *m = a->menu();
        if( a->parent() == menu ) delete a;
        else menu->removeAction( a );
        if( m && m->parent() == menu ) delete m;
    }

    action = menu->addAction(
#ifndef __APPLE__
            ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_playlist.svg", menu ),
#endif
            qtr( "Play &Queue" ));
    action->setShortcut(QString( "Ctrl+L" ));
    action->setCheckable( true );
    connect( action, &QAction::triggered, mi->getPlayqueuePanel(), &SidePanelCtx::setVisible );
    action->setChecked( mi->getPlayqueuePanel()->isVisible() );

    /* Docked Play Queue */
    action = menu->addAction( qtr( "Docked Play Queue" ) );
    action->setCheckable( true );
    connect( action, &QAction::triggered, mi->getPlayqueuePanel(), &SidePanelCtx::setDocked );
    action->setChecked( mi->getPlayqueuePanel()->isDocked() );

    menu->addSeparator();

    action = menu->addAction( qtr( "Always on &top" ) );
    action->setCheckable( true );
    action->setChecked( mi->isInterfaceAlwaysOnTop() );
    connect( action, &QAction::triggered, mi, &MainCtx::setInterfaceAlwaysOnTop );

    menu->addSeparator();

    {
        bool isPlayerVisible = mi->getMainInterfaceModes() & MainCtx::MAININTERFACE_MODE_PLAYER;
        action = menu->addAction(qtr("&Big Player View"));
        action->setCheckable( true );
        action->setChecked( isPlayerVisible );
        connect( action, &QAction::triggered, mi, &MainCtx::setPlayerView);
    }

    /* FullScreen View */
    action = menu->addAction( qtr( "&Fullscreen Interface" ), mi,
            &MainCtx::toggleInterfaceFullScreen );
    action->setShortcut( QString( "F11" ) );
    action->setCheckable( true );
    action->setChecked( mi->interfaceVisibility() == QWindow::FullScreen );

    /*Minimal View*/
    action = menu->addAction( qtr( "&Hide UI" ) );
    action->setShortcut(QString( "Ctrl+\\" ));
    action->setCheckable( true );
    connect(action, &QAction::triggered, mi, &MainCtx::setMinimalView, Qt::QueuedConnection);
    action->setChecked( mi->isMinimalView() );

    action = menu->addAction( qtr( "&View Items as Grid" ), mi,
            &MainCtx::setGridView );
    action->setCheckable( true );
    action->setChecked( mi->hasGridView() );

    menu->addMenu( new CheckableListMenu(qtr( "&Color Scheme" ), mi->getColorScheme(), QActionGroup::ExclusionPolicy::Exclusive, menu) );

    menu->addSeparator();

    InterfacesMenu( p_intf, menu );
    menu->addSeparator();

    /* Extensions */
    ExtensionsMenu( p_intf, menu );
}

/**
 * Interface Sub-Menu, to list extras interface and skins
 **/
void VLCMenuBar::InterfacesMenu( qt_intf_t *p_intf, QMenu *current )
{
    assert(current);
    VLCVarChoiceModel* model = new VLCVarChoiceModel(VLC_OBJECT(p_intf->intf), "intf-add", current);
    CheckableListMenu* submenu = new CheckableListMenu(qtr("Interfaces"), model, QActionGroup::ExclusionPolicy::None, current);
    current->addMenu(submenu);
}

/**
 * Extensions menu: populate the current menu with extensions
 **/
void VLCMenuBar::ExtensionsMenu( qt_intf_t *p_intf, QMenu *extMenu )
{
    /* Get ExtensionsManager and load extensions if needed */
    ExtensionsManager *extMgr = ExtensionsManager::getInstance( p_intf );

    if( !var_InheritBool( p_intf, "qt-autoload-extensions")
        && !extMgr->isLoaded() )
    {
        return;
    }

    if( !extMgr->isLoaded() && !extMgr->cannotLoad() )
    {
        extMgr->loadExtensions();
    }

    /* Let the ExtensionsManager build itself the menu */
    extMenu->addSeparator();
    extMgr->menu( extMenu );
}

void VLCMenuBar::VolumeEntries( qt_intf_t *p_intf, QMenu *current )
{
    current->addSeparator();

    current->addAction( QIcon( ":/menu/volume-high.svg" ), qtr( "&Increase Volume" ), THEMIM, &PlayerController::setVolumeUp );
    current->addAction( QIcon( ":/menu/volume-low.svg" ), qtr( "&Decrease Volume" ), THEMIM, &PlayerController::setVolumeDown );
    current->addAction( QIcon( ":/menu/volume-muted.svg" ), qtr( "&Mute" ), THEMIM, &PlayerController::toggleMuted );
}

/**
 * Main Audio Menu
 **/
void VLCMenuBar::AudioMenu( qt_intf_t *p_intf, QMenu * current )
{
    if( current->isEmpty() )
    {
        current->addMenu(new CheckableListMenu(qtr( "Audio &Track" ), THEMIM->getAudioTracks(), QActionGroup::ExclusionPolicy::ExclusiveOptional, current));

        current->addMenu(new CheckableListMenu(qtr( "&Audio Device" ), THEMIM->getAudioDevices(), QActionGroup::ExclusionPolicy::Exclusive, current));

        VLCVarChoiceModel *mix_mode = THEMIM->getAudioMixMode();
        if (mix_mode->rowCount() == 0)
            current->addMenu( new CheckableListMenu(qtr( "&Stereo Mode" ), THEMIM->getAudioStereoMode(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        else
            current->addMenu( new CheckableListMenu(qtr( "&Mix Mode" ), mix_mode, QActionGroup::ExclusionPolicy::Exclusive, current) );
        current->addSeparator();

        current->addMenu( new CheckableListMenu(qtr( "&Visualizations" ), THEMIM->getAudioVisualizations(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        VolumeEntries( p_intf, current );
    }
}

/* Subtitles */
void VLCMenuBar::SubtitleMenu( qt_intf_t *p_intf, QMenu *current, bool b_popup )
{
    if( current->isEmpty() || b_popup )
    {
        addDPStaticEntry( current, qtr( "Add &Subtitle File..." ), "",
                &DialogsProvider::loadSubtitlesFile);
        current->addMenu(new CheckableListMenu(qtr( "Sub &Track" ), THEMIM->getSubtitleTracks(), QActionGroup::ExclusionPolicy::ExclusiveOptional, current));
        current->addSeparator();
    }
}

/**
 * Main Video Menu
 * Subtitles are part of Video.
 **/
void VLCMenuBar::VideoMenu( qt_intf_t *p_intf, QMenu *current )
{
    if( current->isEmpty() )
    {
        current->addMenu(new CheckableListMenu(qtr( "Video &Track" ), THEMIM->getVideoTracks(), QActionGroup::ExclusionPolicy::None, current));

        current->addSeparator();
        current->addMenu( new CheckableListMenu(qtr( "&3D Output" ), THEMIM->getVideoStereoMode(), QActionGroup::ExclusionPolicy::Exclusive, current) );

        current->addSeparator();
        /* Surface modifiers */
        current->addAction(new BooleanPropertyAction(qtr( "&Fullscreen"), THEMIM, "fullscreen", current));
        QAction* action = new BooleanPropertyAction(qtr( "Always Fit &Window"), THEMIM, "autoscale", current);
        action->setEnabled( THEMIM->hasVideoOutput() );
        connect(THEMIM, &PlayerController::hasVideoOutputChanged, action, &QAction::setEnabled);
        current->addAction(action);
        current->addAction(new BooleanPropertyAction(qtr( "Set as Wall&paper"), THEMIM, "wallpaperMode", current));

        current->addSeparator();
        /* Size modifiers */
        current->addMenu( new CheckableListMenu(qtr( "&Zoom" ), THEMIM->getZoom(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        current->addMenu( new CheckableListMenu(qtr( "&Aspect Ratio" ), THEMIM->getAspectRatio(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        current->addMenu( new CheckableListMenu(qtr( "&Crop" ), THEMIM->getCrop(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        current->addMenu( new CheckableListMenu(qtr( "&Fit" ), THEMIM->getFit(), QActionGroup::ExclusionPolicy::Exclusive, current) );

        current->addSeparator();
        /* Rendering modifiers */
        current->addMenu( new CheckableListMenu(qtr( "&Deinterlace" ), THEMIM->getDeinterlace(), QActionGroup::ExclusionPolicy::Exclusive, current) );
        current->addMenu( new CheckableListMenu(qtr( "&Deinterlace mode" ), THEMIM->getDeinterlaceMode(), QActionGroup::ExclusionPolicy::Exclusive, current) );

        current->addSeparator();
        /* Other actions */
        QAction* snapshotAction = new QAction(qtr( "Take &Snapshot" ), current);
        connect(snapshotAction, &QAction::triggered, THEMIM, &PlayerController::snapshot);
        current->addAction(snapshotAction);
    }
}

/**
 * Navigation Menu
 * For DVD, MP4, MOV and other chapter based format
 **/
void VLCMenuBar::NavigMenu( qt_intf_t *p_intf, QMenu *menu )
{
    QAction *action;
    QMenu *submenu;

    menu->addMenu( new CheckableListMenu( qtr( "T&itle" ), THEMIM->getTitles(), QActionGroup::ExclusionPolicy::Exclusive, menu) );
    submenu = new CheckableListMenu( qtr( "&Chapter" ), THEMIM->getChapters(), QActionGroup::ExclusionPolicy::Exclusive, menu );
    submenu->setTearOffEnabled( true );
    menu->addMenu( submenu );
    menu->addMenu( new CheckableListMenu( qtr("&Program") , THEMIM->getPrograms(), QActionGroup::ExclusionPolicy::Exclusive, menu) );

    MainCtx * mi = p_intf->p_mi;

    if (mi && p_intf->p_mi->hasMediaLibrary() )
    {
        MediaLib * mediaLib = mi->getMediaLibrary();

        BookmarkMenu * bookmarks = new BookmarkMenu(mediaLib, p_intf->p_player, menu);

        bookmarks->setTitle(qfut(I_MENU_BOOKMARK));

        action = menu->addMenu(bookmarks);

        action->setData("bookmark");
    }

    menu->addSeparator();

    if ( !VLCMenuBar::rendererMenu )
        VLCMenuBar::rendererMenu = new RendererMenu( NULL, p_intf, p_intf->p_mainPlayerController );

    menu->addMenu( VLCMenuBar::rendererMenu );
    menu->addSeparator();


    PopupMenuControlEntries( menu, p_intf );

    RebuildNavigMenu( p_intf, menu );
}

void VLCMenuBar::RebuildNavigMenu( qt_intf_t *p_intf, QMenu *menu )
{
    QAction* action;

#define ADD_ACTION( title, slot, visibleSignal, visible ) \
    action = menu->addAction( title, THEMIM,  slot ); \
    if (! visible)\
        action->setVisible(false); \
    connect( THEMIM, visibleSignal, action, &QAction::setVisible );

    ADD_ACTION( qtr("Previous Title"), &PlayerController::titlePrev, &PlayerController::hasTitlesChanged, THEMIM->hasTitles() );
    ADD_ACTION( qtr("Next Title"), &PlayerController::titleNext, &PlayerController::hasTitlesChanged, THEMIM->hasTitles() );
    ADD_ACTION( qtr("Previous Chapter"), &PlayerController::chapterPrev, &PlayerController::hasChaptersChanged, THEMIM->hasChapters() );
    ADD_ACTION( qtr("Next Chapter"), &PlayerController::chapterNext, &PlayerController::hasChaptersChanged, THEMIM->hasChapters() );

#undef ADD_ACTION

    PopupMenuPlaylistEntries( menu, p_intf );
}

/**
 * Help/About Menu
**/
void VLCMenuBar::HelpMenu(qt_intf_t *p_intf, QMenu *menu )
{
    addDPStaticEntry( menu, qtr( "&Help" ) ,
        ":/menu/help.svg", &DialogsProvider::helpDialog, "F1" );
#ifdef UPDATE_CHECK
    addDPStaticEntry( menu, qtr( "Check for &Updates..." ) , "",
                     [ctx = QPointer(p_intf->p_mi)]() {
        if (Q_LIKELY(ctx))
        {
            const auto updateModel = ctx->getUpdateModel();
            assert(updateModel);
            updateModel->checkUpdate(true);
        }
    });
#endif
    menu->addSeparator();
    addDPStaticEntry( menu, qfut( I_MENU_ABOUT ), ":/menu/info.svg",
            &DialogsProvider::aboutDialog, "Shift+F1", QAction::AboutRole );
}

/*****************************************************************************
 * Popup menus - Right Click menus                                           *
 *****************************************************************************/

void VLCMenuBar::PopupMenuPlaylistEntries( QMenu *menu, qt_intf_t *p_intf )
{
    QAction *action;
    bool hasInput = THEMIM->hasInput();

    /* Play or Pause action and icon */
    if( !hasInput || THEMIM->getPlayingState() != PlayerController::PLAYING_STATE_PLAYING )
    {
        action = menu->addAction( qtr( "&Play" ), [p_intf]() {
            if ( THEMPL->isEmpty() )
                THEDP->openFileDialog();
            else
                THEMPL->togglePlayPause();
        });
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( QStringLiteral(":/menu/ic_fluent_play_filled.svg"), menu ) );
#endif
    }
    else
    {
        action = addMPLStaticEntry( p_intf, menu, qtr( "Pause" ),
                ":/menu/ic_pause_filled.svg", &PlaylistController::togglePlayPause );
    }

    /* Stop */
    action = addMPLStaticEntry( p_intf, menu, qtr( "&Stop" ),
            ":/menu/ic_fluent_stop.svg", &PlaylistController::stop );
    if( !hasInput )
        action->setEnabled( false );

    /* Next / Previous */
    bool bPlaylistEmpty = THEMPL->isEmpty();
    QAction* previousAction = addMPLStaticEntry( p_intf, menu, qtr( "Pre&vious" ),
            ":/menu/ic_fluent_previous.svg", &PlaylistController::prev );
    previousAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistController::isEmptyChanged, previousAction, &QAction::setDisabled);

    QAction* nextAction = addMPLStaticEntry( p_intf, menu, qtr( "Ne&xt" ),
            ":/menu/ic_fluent_next.svg", &PlaylistController::next );
    nextAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistController::isEmptyChanged, nextAction, &QAction::setDisabled);

    auto playAction = menu->actions()[menu->actions().size() - 4];
    auto stopAction = menu->actions()[menu->actions().size() - 3];
    menuIcon(playAction, THEMIM->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING ? "pause" : "play");
    menuIcon(stopAction, "stop");
    menuIcon(previousAction, "previous");
    menuIcon(nextAction, "next");
    configuredShortcut(p_intf, playAction, "key-play-pause");
    configuredShortcut(p_intf, stopAction, "key-stop");
    previousAction->setText(qtr("Previous Item"));
    nextAction->setText(qtr("Next Item"));
    configuredShortcut(p_intf, previousAction, "key-prev");
    configuredShortcut(p_intf, nextAction, "key-next");

    action = menu->addAction( qtr( "Record" ), THEMIM, &PlayerController::toggleRecord );
    configuredShortcut(p_intf, action, "key-record");
    menuIcon(action, "record");
    action->setCheckable(true);
    action->setChecked(THEMIM->isRecording());
    action->setIcon( QIcon( ":/menu/ic_fluent_record.svg" ) );
    if( !hasInput )
        action->setEnabled( false );
    menu->addSeparator();
}

void VLCMenuBar::PopupMenuControlEntries( QMenu *menu, qt_intf_t *p_intf,
                                        bool b_normal )
{
    QAction *action;
    QMenu *rateMenu = new QMenu( qtr( "Sp&eed" ), menu );
    rateMenu->setTearOffEnabled( true );

    if( b_normal )
    {
        /* Faster/Slower */
        action = rateMenu->addAction( qtr( "&Faster" ), THEMIM,
                                  &PlayerController::faster );
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_fast_forward.svg", rateMenu ) );
#endif
    }

    action = rateMenu->addAction( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_fast_forward.svg", rateMenu ), qtr( "Faster (fine)" ), THEMIM,
                              &PlayerController::littlefaster );

    action = rateMenu->addAction( qtr( "N&ormal Speed" ), THEMIM,
                              &PlayerController::normalRate );

    action = rateMenu->addAction( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_rewind.svg", rateMenu ), qtr( "Slower (fine)" ), THEMIM,
                              &PlayerController::littleslower );

    if( b_normal )
    {
        action = rateMenu->addAction( qtr( "Slo&wer" ), THEMIM,
                                  &PlayerController::slower );
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_rewind.svg", rateMenu ) );
#endif
    }

    action = menu->addMenu( rateMenu );

    menu->addSeparator();

    if( !b_normal ) return;

    action = menu->addAction( qtr( "&Jump Forward" ), THEMIM,
             &PlayerController::jumpFwd );
#ifndef __APPLE__ /* No icons in menus in Mac */
    action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_skip_forward_10.svg", menu ) );
#endif

    action = menu->addAction( qtr( "Jump Bac&kward" ), THEMIM,
             &PlayerController::jumpBwd );
#ifndef __APPLE__ /* No icons in menus in Mac */
    action->setIcon( ColorizedSvgIcon::colorizedIconForWidget( ":/menu/ic_fluent_skip_back_10.svg", menu ) );
#endif

    action = menu->addAction( qfut( I_MENU_GOTOTIME ), THEDP, &DialogsProvider::gotoTimeDialog );
    action->setShortcut( QKeySequence( "Ctrl+T" ) );

    menu->addSeparator();
}

void VLCMenuBar::PopupMenuStaticEntries( QMenu *menu )
{
    QMenu *openmenu = new QMenu( qtr( "Open Media" ), menu );
    addDPStaticEntry( openmenu, qfut( I_OP_OPF ),
        ":/menu/file.svg", &DialogsProvider::openFileDialog);
    addDPStaticEntry( openmenu, qfut( I_OP_OPDIR ),
        ":/menu/folder.svg", &DialogsProvider::PLOpenDir);
    addDPStaticEntry( openmenu, qtr( "Open &Disc..." ),
        ":/menu/disc.svg", &DialogsProvider::openDiscDialog);
    addDPStaticEntry( openmenu, qtr( "Open &Network..." ),
        ":/menu/network.svg", &DialogsProvider::openNetDialog);
    addDPStaticEntry( openmenu, qtr( "Open &Capture Device..." ),
        ":/menu/capture-card.svg", &DialogsProvider::openCaptureDialog);
    menu->addMenu( openmenu );

    menu->addSeparator();

    addDPStaticEntry( menu, qtr( "Quit" ), ":/menu/exit.svg",
                      &DialogsProvider::quit, "Ctrl+Q", QAction::QuitRole );
}

/* Video Tracks and Subtitles tracks */
QMenu* VLCMenuBar::VideoPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    VideoMenu(p_intf, menu);
    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Audio Tracks */
QMenu* VLCMenuBar::AudioPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    AudioMenu(p_intf, menu);
    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Navigation stuff, and general menus ( open ), used only for skins */
QMenu* VLCMenuBar::MiscPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);

    menu->addSeparator();
    PopupMenuPlaylistEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuControlEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuStaticEntries( menu );

    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Main Menu that sticks everything together  */
QMenu* VLCMenuBar::PopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    if (!p_intf->b_isDialogProvider)
    {
        PopupMenuPlaylistEntries(menu, p_intf);
        auto hide = menu->addAction(qtr("&Hide UI"));
        hide->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));
        hide->setCheckable(true);
        hide->setChecked(p_intf->p_mi->isMinimalView());
        connect(hide, &QAction::triggered, p_intf->p_mi, &MainCtx::setMinimalView, Qt::QueuedConnection);
        menuIcon(hide, "hide");
        menu->addSeparator();
        auto playback = menu->addMenu(qtr("Playback"));
        NavigMenu(p_intf, playback);
        menuIcon(playback->menuAction(), "playback");
        auto video = menu->addMenu(qtr("Video"));
        VideoMenu(p_intf, video);
        menuIcon(video->menuAction(), "video");
        auto audio = menu->addMenu(qtr("Audio"));
        AudioMenu(p_intf, audio);
        menuIcon(audio->menuAction(), "audio");
        auto subtitle = menu->addMenu(qtr("Subtitle"));
        SubtitleMenu(p_intf, subtitle);
        menuIcon(subtitle->menuAction(), "subtitle");
        menu->addSeparator();
        auto settings = menu->addMenu(qtr("More Settings"));
        ToolsMenu(p_intf, settings->addMenu(qtr("Tools")));
        ViewMenu(p_intf, settings->addMenu(qtr("View")));
        HelpMenu(p_intf, settings->addMenu(qtr("Help")));
        menuIcon(settings->menuAction(), "settings");
        auto playlist = menu->addMenu(qtr("Playlist"));
        menuIcon(playlist->menuAction(), "playlist");
        auto queue = playlist->addAction(qtr("Play Queue"));
        queue->setCheckable(true);
        queue->setChecked(p_intf->p_mi->getPlayqueuePanel()->isVisible());
        queue->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
        connect(queue, &QAction::triggered, p_intf->p_mi->getPlayqueuePanel(), &SidePanelCtx::setVisible);
        auto docked = playlist->addAction(qtr("Docked Play Queue"));
        docked->setCheckable(true);
        docked->setChecked(p_intf->p_mi->getPlayqueuePanel()->isDocked());
        connect(docked, &QAction::triggered, p_intf->p_mi->getPlayqueuePanel(), &SidePanelCtx::setDocked);
        addDPStaticEntry(playlist, qfut(I_PL_SAVE), "", &DialogsProvider::savePlayingToPlaylist, "Ctrl+Y");
        playlist->addAction(qtr("Shuffle the play queue"), THEMPL, &PlaylistController::shuffle);
        playlist->addAction(qtr("Clear the play queue"), THEMPL, &PlaylistController::clear);
        auto open = menu->addMenu(qtr("Open Media"));
        FileMenu(p_intf, open);
        menuIcon(open->menuAction(), "file");
        auto quit = addDPStaticEntry(menu, qtr("Quit VLC"), "", &DialogsProvider::quit, "Ctrl+Q", QAction::QuitRole);
        menuIcon(quit, "quit");
        styleMenu(p_intf, menu, 292);
        if (show) menu->popup(QCursor::pos());
        return menu;
    }
    // Legacy skins/dialog-provider menu remains available with skin commands.
    input_item_t* p_input = THEMIM->getInput();
    QAction *action;
    bool b_isFullscreen = false;

    action = menu->addAction(qtr("&Hide UI"));
    action->setShortcut(QKeySequence(QStringLiteral("Ctrl+\\")));
    action->setCheckable(true);
    action->setChecked(p_intf->p_mi->isMinimalView());
    connect(action, &QAction::triggered, p_intf->p_mi, &MainCtx::setMinimalView, Qt::QueuedConnection);
    menu->addSeparator();

    PopupMenuPlaylistEntries( menu, p_intf );
    menu->addSeparator();

    if( p_input )
    {
        QMenu *submenu;
        SharedVOutThread p_vout = THEMIM->getVout();

        /* Add a fullscreen switch button, since it is the most used function */
        if( p_vout )
        {
            menu->addAction(new BooleanPropertyAction(qtr( "&Fullscreen" ), THEMIM, "fullscreen", menu) );
            menu->addSeparator();
        }

        /* Input menu */

        /* Audio menu */
        submenu = new QMenu( menu );
        AudioMenu( p_intf, submenu );
        action = menu->addMenu( submenu );
        action->setText( qtr( "&Audio" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );

        /* Video menu */
        submenu = new QMenu( menu );
        VideoMenu( p_intf, submenu );
        action = menu->addMenu( submenu );
        action->setText( qtr( "&Video" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );

        /* Subtitles menu */
        submenu = new QMenu( menu );
        SubtitleMenu( p_intf, submenu, true );
        action = menu->addMenu( submenu );
        action->setText( qtr( "Subti&tle") );

        /* Playback menu for chapters */
        submenu = new QMenu( menu );
        NavigMenu( p_intf, submenu );
        action = menu->addMenu( submenu );
        action->setText( qtr( "&Playback" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );
    }

    menu->addSeparator();

    /* Add some special entries for windowed mode: Interface Menu */
    if( !b_isFullscreen )
    {
        if( p_intf->b_isDialogProvider )
        {
            // same as Tool menu but with extra entries
            QMenu* submenu = new QMenu( qtr( "Interface" ), menu );
            ToolsMenu( p_intf, submenu );
            submenu->addSeparator();

            vlc_object_t* p_object = vlc_object_parent(p_intf->intf);

            /* Open skin dialog box */
            if (var_Type(p_object, "intf-skins-interactive") & VLC_VAR_ISCOMMAND)
            {
                QAction* openSkinAction = new QAction(qtr("Open skin..."), menu);
                openSkinAction->setShortcut( QKeySequence( "Ctrl+Shift+S" ));
                connect(openSkinAction, &QAction::triggered, [=]() {
                    var_TriggerCallback(p_object, "intf-skins-interactive");
                });
                submenu->addAction(openSkinAction);
            }

            VLCVarChoiceModel* skinmodel = new VLCVarChoiceModel(p_object, "intf-skins", submenu);
            CheckableListMenu* skinsubmenu = new CheckableListMenu(qtr("Interface"), skinmodel, QActionGroup::ExclusionPolicy::ExclusiveOptional, submenu);
            submenu->addMenu(skinsubmenu);

            submenu->addSeparator();

            /* list of extensions */
            ExtensionsMenu( p_intf, submenu );

            menu->addMenu( submenu );
        }
        else
        {
            QMenu* toolsMenu = new QMenu( qtr( "Tool&s" ), menu );
            ToolsMenu( p_intf, toolsMenu );
            menu->addMenu( toolsMenu );

            QMenu* viewMenu = new QMenu( qtr( "V&iew" ), menu );
            ViewMenu( p_intf, viewMenu );
            menu->addMenu( viewMenu );
        }
    }

    /* Static entries for ending, like open */
    if( p_intf->b_isDialogProvider )
    {

        QMenu* openmenu = new QMenu( qtr( "Open Media" ), menu );
        FileMenu( p_intf, openmenu );
        menu->addMenu( openmenu );

        menu->addSeparator();

        QMenu* helpmenu = new QMenu( qtr( "Help" ), menu );
        HelpMenu( p_intf, helpmenu );
        menu->addMenu( helpmenu );

        addDPStaticEntry( menu, qtr( "Quit" ), ":/menu/exit.svg",
                          &DialogsProvider::quit, "Ctrl+Q", QAction::QuitRole );
    }
    else
        PopupMenuStaticEntries( menu );

    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}
