/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *****************************************************************************/
#include "vlcfonts.hpp"

#include <QApplication>
#include <QDebug>
#include <QFontDatabase>
#include <QFontInfo>
#include <QQmlFile>

namespace {

void loadApplicationFont(const char *resource)
{
    const QString path = QQmlFile::urlToLocalFileOrQrc(QString::fromLatin1(resource));
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
    {
        qWarning().noquote() << "VLC fonts: failed to load" << resource;
        return;
    }

    qInfo().noquote() << "VLC fonts: loaded" << resource
                      << "families=" << QFontDatabase::applicationFontFamilies(id).join(u", ");
}

void logInterWeight(QFont::Weight weight)
{
    QFont font(QStringLiteral("Inter"));
    font.setWeight(weight);
    const QFontInfo info(font);
    qInfo().noquote() << "VLC fonts: Inter requestedWeight=" << static_cast<int>(weight)
                      << "resolvedFamily=" << info.family()
                      << "resolvedStyle=" << info.styleName()
                      << "resolvedWeight=" << info.weight();
}

}

void initializeVLCFonts()
{
    loadApplicationFont("qrc:///fonts/Inter-Light.ttf");
    loadApplicationFont("qrc:///fonts/Inter-Regular.ttf");
    loadApplicationFont("qrc:///fonts/Inter-Medium.ttf");
    loadApplicationFont("qrc:///fonts/Inter-SemiBold.ttf");
    loadApplicationFont("qrc:///fonts/Inter-Bold.ttf");
    loadApplicationFont("qrc:///fonts/DMMono-Regular.ttf");

    QFont applicationFont(QStringLiteral("Inter"));
    applicationFont.setWeight(QFont::Normal);
    QApplication::setFont(applicationFont);

    for (const auto weight : { QFont::Light, QFont::Normal, QFont::Medium,
                               QFont::DemiBold, QFont::Bold })
        logInterWeight(weight);

    const QFontInfo dmMono(QFont(QStringLiteral("DM Mono")));
    qInfo().noquote() << "VLC fonts: DM Mono requestedWeight=" << static_cast<int>(QFont::Normal)
                      << "resolvedFamily=" << dmMono.family()
                      << "resolvedStyle=" << dmMono.styleName()
                      << "resolvedWeight=" << dmMono.weight();
}
