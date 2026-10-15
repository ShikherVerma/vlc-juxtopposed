/*****************************************************************************
 * vlc-qt-check.cpp: run-time Qt availability test
 ****************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cstdlib>
#include <QApplication>
#include <QTextStream>
#include <QtGlobal>
#include <iostream>

static void messageOutput(QtMsgType type, const QMessageLogContext &,
                          const QString &msg)
{
    if (type == QtFatalMsg)
    {
        std::cerr << msg.toUtf8().constData() << std::endl;
        exit(1);
    }
}

#include <qconfig.h>
#include <QtPlugin>
QT_BEGIN_NAMESPACE
#include "plugins.hpp"
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageOutput);
#ifdef Q_OS_MACOS
    // A bundled sanity-check child must not take the parent's Cocoa delegate,
    // launch-document events or native menu ownership. Still construct the real
    // QApplication/platform plugin and preserve its ordinary failure status.
    QCoreApplication::setAttribute(Qt::AA_PluginApplication);
#endif
    QApplication app(argc, argv);
}
