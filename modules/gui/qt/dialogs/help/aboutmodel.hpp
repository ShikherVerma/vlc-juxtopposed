/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#ifndef ABOUTMODEL_HPP
#define ABOUTMODEL_HPP

#include <QObject>
#include <QtQml>

#include <vlc_about.h>
#include "config.h"

class AboutModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString license READ getLicense CONSTANT FINAL)
    Q_PROPERTY(QString authors READ getAuthors CONSTANT FINAL)
    Q_PROPERTY(QString thanks  READ getThanks  CONSTANT FINAL)
    Q_PROPERTY(QString version  READ getVersion  CONSTANT FINAL)

    QML_ELEMENT
    QML_UNCREATABLE("AboutModel is meant to be a singleton.")
    QML_SINGLETON

public:
    explicit AboutModel(QObject *parent = nullptr)
        : QObject(parent)
    { }

    static AboutModel *create(class QQmlEngine *, class QJSEngine *)
    {
        return new AboutModel;
    }

    // USER: fill in the public URL of this fork's complete corresponding source
    // (including packaging scripts) before distributing a release.
    static constexpr const char *SOURCE_URL = "";

    static QString getCredits()
    {
        QString text = tr("<p>Design: Juxtopposed "
                          "(<a href=\"https://www.youtube.com/watch?v=ERFeOgqk324\">YouTube video</a> · "
                          "<a href=\"https://www.figma.com/community/file/1691465999134370980/vlc-redesign\">Figma file</a>)</p>"
                          "<p>Built on VLC media player by VideoLAN and contributors</p>"
                          "<p>GNU General Public License, version 2 or later (GPLv2+).</p>");
        const QString source = QString::fromUtf8(SOURCE_URL);
        if (source.isEmpty())
            text += tr("<p>Source code: source available on request from the distributor. "
                       "Release maintainer: fill in SOURCE_URL in dialogs/help/aboutmodel.hpp.</p>");
        else
            text += tr("<p>Source code: <a href=\"%1\">%1</a></p>").arg(source.toHtmlEscaped());
        return text;
    }

    static QString getLicense()
    {
        return QString::fromUtf8(psz_license);
    }

    static QString getAuthors()
    {
        return QString::fromUtf8(psz_authors);
    }

    static QString getThanks()
    {
        return QString::fromUtf8(psz_thanks);
    }

    static QString getVersion()
    {
        return QString::fromUtf8(VERSION_MESSAGE);
    }
};

#endif // ABOUTMODEL_HPP
