/*****************************************************************************
 * help.cpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *          Rémi Duraffort <ivoire (at) via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "help.hpp"
#include "aboutmodel.hpp"
#include "util/qt_dirs.hpp"
#include "maininterface/mainctx.hpp"
#include "style/systempalette.hpp"
#include <algorithm>
#include <cmath>

#include <vlc_intf_strings.h>

#ifdef UPDATE_CHECK
# include <vlc_update.h>
#endif

#include <QTextBrowser>
#include <QString>
#include <QDialogButtonBox>
#include <QEvent>
#include <QDate>
#include <QPushButton>

#include <cassert>

#ifndef NDEBUG
// Uncomment the following line to make use a mock update for debugging purposes,
// it is only applicable when there is no new release found:
// #define UPDATE_MOCK
#endif

HelpDialog::HelpDialog( qt_intf_t *_p_intf ) : QVLCFrame( _p_intf )

{
    setWindowTitle( qtr( "Help" ) );
    setWindowRole( "vlc-help" );
    setMinimumSize( 350, 300 );

    QVBoxLayout *layout = new QVBoxLayout( this );

    QTextBrowser *helpBrowser = new QTextBrowser( this );
    helpBrowser->setOpenExternalLinks( true );
    helpBrowser->setHtml( qfut(I_LONGHELP) );

    QDialogButtonBox *closeButtonBox = new QDialogButtonBox( this );
    closeButtonBox->addButton(
        new QPushButton( qtr("&Close") ), QDialogButtonBox::RejectRole );
    closeButtonBox->setFocus();

    layout->addWidget( helpBrowser );
    layout->addWidget( closeButtonBox );

    connect( closeButtonBox, &QDialogButtonBox::rejected, this, &HelpDialog::close );
    restoreWidgetPosition( "Help", QSize( 500, 450 ) );
}

HelpDialog::~HelpDialog()
{
    saveWidgetPosition( "Help" );
}

AboutDialog::AboutDialog( qt_intf_t *_p_intf)
            : QVLCDialog( nullptr, _p_intf ), b_advanced( false )
{
    /* Build UI */
    ui.setupUi( this );
    setWindowTitle( qtr( "About VLC (Juxtopposed)" ) );
    setWindowRole( "vlc-about" );
    setWindowModality( Qt::WindowModal );

    ui.version->setText(qfu( " " VERSION_MESSAGE ) );
    ui.title->setText(qtr("VLC (Juxtopposed)"));
    ui.title->setTextFormat(Qt::PlainText);

    ui.MainBlabla->setText(AboutModel::getCredits());
    ui.MainBlabla->setOpenExternalLinks(true);

    const QDate today = QDate::currentDate();
    if( today.month() == 4 && today.day() == 1
            && var_InheritBool( p_intf, "qt-icon-change" ) )
        ui.VLCcone->setPixmap( QPixmap( ":/logo/vlc128-aprilfools.png" ) );

    ui.update->hide();

    /* GPL License */
    ui.licensePage->setText( AboutModel::getLicense() );

    /* People who helped */
    ui.creditPage->setText( AboutModel::getThanks() );

    /* People who wrote the software */
    ui.authorsPage->setText( AboutModel::getAuthors() );

    ui.licenseButton->setText(qtr("License"));
    ui.licenseButton->setTextFormat(Qt::PlainText);
    ui.licenseButton->installEventFilter( this );

    ui.authorsButton->setText(qtr("Authors"));
    ui.authorsButton->setTextFormat(Qt::PlainText);
    ui.authorsButton->installEventFilter( this );

    ui.creditsButton->setText(qtr("Credits"));
    ui.creditsButton->setTextFormat(Qt::PlainText);
    ui.creditsButton->installEventFilter( this );

    ui.version->installEventFilter( this );

    // Same provider and authored QWidget tokens as the Settings shell.
    m_palette = new SystemPalette(this);
    m_palette->setCtx(p_intf->p_mi);
    m_palette->setSource(p_intf->p_mi ? p_intf->p_mi->getColorScheme()->currentScheme() : ColorSchemeModel::Night);
    connect(m_palette, &SystemPalette::paletteChanged, this, &AboutDialog::applyTheme);
    if (p_intf->p_mi)
        connect(p_intf->p_mi->getColorScheme(), &ColorSchemeModel::currentChanged, this, [this] {
            m_palette->setSource(p_intf->p_mi->getColorScheme()->currentScheme());
        });
    applyTheme();
}

void AboutDialog::applyTheme()
{
    const bool light = !m_palette->property("isDark").toBool();
    const QString canvas = light ? "#e7e7e7" : "#0e0e0e";
    const QString panel = light ? "#d5d5d5" : "#181818";
    const QString fg = light ? "#2e2e2e" : "#8f8f8f";
    const QString primary = light ? "#161616" : "#c6c6c6";
    QColor link = m_palette->get_accentMain();
    // Preserve the selected accent hue, adjusting luminance only when needed
    // for AA small-text contrast on BOTH main and footer surfaces.
    const auto luminance = [](const QColor &color) {
        const auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
    };
    const auto contrast = [&](const QColor &bg) {
        const double a = luminance(link), b = luminance(bg);
        return (std::max(a, b) + .05) / (std::min(a, b) + .05);
    };
    for (int i = 0; i < 100 && (contrast(QColor(canvas)) < 4.5 || contrast(QColor(panel)) < 4.5); ++i)
    {
        link = light ? link.darker(110) : link.lighter(110);
        // Measure the same 8-bit RGB that name() emits into rich text / CSS.
        link = QColor(link.name());
    }
    if (contrast(QColor(canvas)) < 4.5 || contrast(QColor(panel)) < 4.5)
        link = QColor(primary); // e.g. a custom pure-black accent in Dark

    setStyleSheet(QStringLiteral(
        "QWidget { color:%1; font-family:Inter; font-size:14px; background:%2; }"
        "QFrame#footer { background:%3; }"
        "QLabel { background:transparent; }"
        "QLabel#title { color:%4; font-size:26pt; }"
        "QLabel#authorsButton, QLabel#licenseButton, QLabel#creditsButton { color:%5; text-decoration:underline; }"
        "QTextEdit { background:%2; color:%1; border:0; selection-background-color:%5; selection-color:%2; }"
        "QScrollBar:vertical { background:%2; width:6px; margin:0; }"
        "QScrollBar::handle:vertical { background:%1; min-height:32px; border-radius:3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; border:0; background:transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"
    ).arg(fg, canvas, panel, primary, link.name()));
    QString credits = AboutModel::getCredits();
    credits.replace(QStringLiteral("<a "), QStringLiteral("<a style=\"color:%1;\" ").arg(link.name()));
    ui.MainBlabla->setText(credits);
    for (auto *page : {ui.licensePage, ui.authorsPage, ui.creditPage})
    {
        QPalette palette = page->palette();
        palette.setColor(QPalette::Link, link);
        palette.setColor(QPalette::LinkVisited, link);
        page->setPalette(palette);
    }
}

void AboutDialog::showLicense()
{
    ui.stackedWidget->setCurrentWidget( ui.licensePage );
}

void AboutDialog::showAuthors()
{
    ui.stackedWidget->setCurrentWidget( ui.authorsPage );
}

void AboutDialog::showCredit()
{
    ui.stackedWidget->setCurrentWidget( ui.creditPage );
}

bool AboutDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress )
    {
        if( obj == ui.version )
        {
            if( !b_advanced )
            {
                ui.version->setText(qfu( VLC_CompileBy() )+ "@" + qfu( VLC_CompileHost() )
                    + " " + __DATE__ + " " + __TIME__);
                b_advanced = true;
            }
            else
            {
                ui.version->setText(qfu( " " VERSION_MESSAGE ) );
                b_advanced = false;
            }
            return true;
        }
        else if( obj == ui.licenseButton )
            showLicense();
        else if( obj == ui.authorsButton )
            showAuthors();
        else if( obj == ui.creditsButton )
            showCredit();

        return false;
    }

    return QVLCDialog::eventFilter( obj, event);
}

void AboutDialog::showEvent( QShowEvent *event )
{
    ui.stackedWidget->setCurrentWidget( ui.blablaPage );
    QVLCDialog::showEvent( event );
}

#ifdef UPDATE_CHECK

class UpdateModelPrivate
{
public:
    Q_DECLARE_PUBLIC(UpdateModel)
    UpdateModelPrivate(UpdateModel * pub)
        : q_ptr(pub)
    {
    }

    update_t* m_update = nullptr;

    const update_release_t* m_release = nullptr;
    UpdateModel::Status m_status = UpdateModel::Unchecked;
    bool m_explicitCheck = false;

    UpdateModel* q_ptr = nullptr;
};

static void UpdateCallback(void *data, bool b_ret)
{
    auto that = (UpdateModelPrivate*)data;
    QMetaObject::invokeMethod(that->q_func(), [that, b_ret](){
        if (!b_ret)
        {
            that->m_status = UpdateModel::CheckFailed;
            that->m_release = nullptr;
        }
        else
        {
            bool needUpdate = update_NeedUpgrade( that->m_update );
            if (!needUpdate)
            {
#if defined(UPDATE_MOCK) && !defined(NDEBUG)
               static const update_release_t updateReleaseMock = {
                   9,
                   0,
                   99,
                   999,
                   "",
                   // Lorem ipsum with some "Security" to see if red coloring works:
                   "Lorem ipsum dolor sit amet, security consectetur adipiscing elit. Sed vitae ante lobortis," \
                   "condimentum sem et, auctor libero. Aliquam eget mi justo. <br /> <br /> Class aptent taciti sociosqu" \
                   "ad litora Security torquent per conubia nostra, per inceptos himenaeos. Nulla id pretium ante. Nam" \
                   "eu blandit lacus. Proin faucibus in risus quis condimentum. <br /> <br /> Pellentesque a tellus vitae" \
                   "massa tristique cursus. Phasellus fermentum euismod mauris, at ultricies ipsum volutpat eu." \
                   "Praesent arcu lacus, laoreet at lacinia quis, rhoncus at lectus. " \
                   // Repeat:
                   "Lorem ipsum dolor sit amet, security consectetur adipiscing elit. Sed vitae ante lobortis," \
                   "condimentum sem et, auctor libero. Aliquam eget mi justo. <br /> <br /> Class aptent taciti sociosqu" \
                   "ad litora Security torquent per conubia nostra, per inceptos himenaeos. Nulla id pretium ante. Nam" \
                   "eu blandit lacus. Proin faucibus in risus quis condimentum. <br /> <br /> Pellentesque a tellus vitae" \
                   "massa tristique cursus. Phasellus fermentum euismod mauris, at ultricies ipsum volutpat eu." \
                   "Praesent arcu lacus, laoreet at lacinia quis, rhoncus at lectus. " \
                   // Repeat:
                   "Lorem ipsum dolor sit amet, security consectetur adipiscing elit. Sed vitae ante lobortis," \
                   "condimentum sem et, auctor libero. Aliquam eget mi justo. <br /> <br /> Class aptent taciti sociosqu" \
                   "ad litora Security torquent per conubia nostra, per inceptos himenaeos. Nulla id pretium ante. Nam" \
                   "eu blandit lacus. Proin faucibus in risus quis condimentum. <br /> <br /> Pellentesque a tellus vitae" \
                   "massa tristique cursus. Phasellus fermentum euismod mauris, at ultricies ipsum volutpat eu." \
                   "Praesent arcu lacus, laoreet at lacinia quis, rhoncus at lectus. " \
               };

               that->m_release = &updateReleaseMock;
               that->m_status = UpdateModel::NeedUpdate;
#else
               that->m_status = UpdateModel::UpToDate;
               that->m_release = nullptr;
#endif
            }
            else
            {
                that->m_status = UpdateModel::NeedUpdate;
                that->m_release = update_GetRelease(that->m_update);
            }
        }
        emit that->q_func()->updateStatusChanged();
    });
}

UpdateModel::UpdateModel(qt_intf_t * p_intf)
    : d_ptr(new UpdateModelPrivate(this))
{
    Q_D(UpdateModel);
    d->m_update = update_New( p_intf );
}

UpdateModel::~UpdateModel()
{
    Q_D(UpdateModel);
    update_Delete( d->m_update );
}

void UpdateModel::checkUpdate(bool explicitCheck)
{
    Q_D(UpdateModel);
    if (d->m_status == Checking)
        return;

    if (d->m_explicitCheck != explicitCheck)
    {
        d->m_explicitCheck = explicitCheck;
        emit explicitCheckChanged();
    }

    d->m_release = nullptr;
    d->m_status = Checking;
    emit updateStatusChanged();
    update_Check( d->m_update, UpdateCallback, d );
}

bool UpdateModel::download(QString destDir)
{
    Q_D(UpdateModel);
    if (d->m_status != NeedUpdate)
        return false;
    update_Download( d->m_update, qtu( destDir ) );
    return true;
}

bool UpdateModel::download()
{
    QString dest_dir = QDir::tempPath();
    if (Q_UNLIKELY(dest_dir.isEmpty()))
        return false;

    dest_dir = toNativeSepNoSlash( std::move(dest_dir) ) + DIR_SEP;
    qDebug() << "Downloading to folder:" << qtu( dest_dir );

    return download(dest_dir);
}

void UpdateModel::resetStatus()
{
    Q_D(UpdateModel);
    if (d->m_status == Unchecked)
        return;
    d->m_status = Unchecked;
    emit updateStatusChanged();
}

UpdateModel::Status UpdateModel::updateStatus() const
{
    Q_D(const UpdateModel);
    return d->m_status;
}

int UpdateModel::getMajor() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_major;
}
int UpdateModel::getMinor() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_minor;
}
int UpdateModel::getRevision() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_revision;
}
int UpdateModel::getExtra() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return d->m_release->i_extra;
}
QString UpdateModel::getDescription() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return qfu( d->m_release->psz_desc );
}
QString UpdateModel::getUrl() const
{
    Q_D(const UpdateModel);
    if (!d->m_release) return 0;
    return qfu( d->m_release->psz_desc );
}

double UpdateModel::getProgress() const
{
    // TODO: Stub
    return 0.0;
}

bool UpdateModel::explicitCheck() const
{
    Q_D(const UpdateModel);
    return d->m_explicitCheck;
}

void UpdateModel::resetExplicitCheck()
{
    Q_D(UpdateModel);
    if (!d->m_explicitCheck)
        return;
    d->m_explicitCheck = false;
    emit explicitCheckChanged();
}

/*****************************************************************************
 * UpdateDialog
 *****************************************************************************/

UpdateDialog::UpdateDialog( qt_intf_t *_p_intf ) : QVLCFrame( _p_intf )
{
    /* build Ui */
    ui.setupUi( this );
    ui.updateDialogButtonBox->addButton( new QPushButton( qtr("&Close"), this ),
                                         QDialogButtonBox::RejectRole );
    QPushButton *recheckButton = new QPushButton( qtr("&Recheck version"), this );
    ui.updateDialogButtonBox->addButton( recheckButton, QDialogButtonBox::ActionRole );

    ui.updateNotifyButtonBox->addButton( new QPushButton( qtr("&Yes"), this ),
                                         QDialogButtonBox::AcceptRole );
    ui.updateNotifyButtonBox->addButton( new QPushButton( qtr("&No"), this ),
                                         QDialogButtonBox::RejectRole );

    setWindowTitle( qtr( "VLC media player updates" ) );
    setWindowRole( "vlc-update" );

    BUTTONACT( recheckButton, &UpdateDialog::checkOrDownload );
    connect( ui.updateDialogButtonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::close );

    connect( ui.updateNotifyButtonBox, &QDialogButtonBox::accepted, this, &UpdateDialog::checkOrDownload );
    connect( ui.updateNotifyButtonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::close );

    setMinimumSize( 300, 300 );
    setMaximumSize( 500, 300 );

    restoreWidgetPosition( "Update", maximumSize() );

    m_model = p_intf->p_mi->getUpdateModel();
    connect(m_model, &UpdateModel::updateStatusChanged, this, &UpdateDialog::updateUI);
    /* update status*/
    updateUI();
}

UpdateDialog::~UpdateDialog()
{
    saveWidgetPosition( "Update" );
}

bool UpdateDialog::event(QEvent* event)
{
    assert(event);

    if (Q_LIKELY(m_model))
    {
        switch(event->type())
        {
        case QEvent::Hide:
        case QEvent::Show:
        case QEvent::Close:
        case QEvent::Destroy:
            m_model->resetExplicitCheck();
            break;
        default:
            break;
        }
    }

    return QVLCFrame::event(event);
}

/* Check for updates */
void UpdateDialog::checkOrDownload()
{
    switch (m_model->updateStatus()) {
    case UpdateModel::Unchecked:
    case UpdateModel::UpToDate:
    case UpdateModel::CheckFailed:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateRequestPage );
        m_model->checkUpdate();
        break;
    }
    case UpdateModel::NeedUpdate:
    {
        if (m_model->download())
            toggleVisible();
        break;
    }
    default: // Checking
        break;
    }
}

/* Notify the end of the update_Check */
void UpdateDialog::updateUI( )
{
    switch (m_model->updateStatus()) {
    case UpdateModel::NeedUpdate:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateNotifyPage );
        int extra = m_model->getExtra();
        QString message = QString(
                              qtr( "A new version of VLC (%1.%2.%3%4) is available." ) )
                              .arg( m_model->getMajor() )
                              .arg( m_model->getMinor() )
                              .arg( m_model->getRevision()  )
                              .arg( extra == 0 ? QStringLiteral("") : QStringLiteral(".") + QString::number( extra ) );

        ui.updateNotifyLabel->setText( message );
        message = m_model->getDescription().replace( "\n", "<br/>" );

        /* Try to highlight releases featuring security changes */
        int i_index = message.indexOf( "security", Qt::CaseInsensitive );
        if ( i_index >= 0 )
        {
            message.insert( i_index + 8, "</font>" );
            message.insert( i_index, "<font style=\"color:red\">" );
        }
        ui.updateNotifyTextEdit->setHtml( message );
        break;
    }
    case UpdateModel::UpToDate:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "You have the latest version of VLC media player." ) );
        break;
    }
    case UpdateModel::CheckFailed:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "An error occurred while checking for updates..." ) );
        break;
    }
    case UpdateModel::Checking:
    {
        ui.stackedWidget->setCurrentWidget( ui.updateDialogPage );
        ui.updateDialogLabel->setText(
            qtr( "Checking for updates..." ) );
        break;
    }
    case UpdateModel::Unchecked:
        // do nothing
        break;
    case UpdateModel::Downloading:
        // NOTE: It is not planned to implement this in the legacy dialog.
        //       The new update pane already respects it, we are only
        //       waiting for the core to provide this information.
        break;
    }

    if (isVisible())
    {
        if (const auto w = windowHandle())
        {
            w->alert(0);
        }
    }
}

#endif
