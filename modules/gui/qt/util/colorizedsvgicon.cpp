/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include "colorizedsvgicon.hpp"

#include <QPluginLoader>
#include <QIconEnginePlugin>
#include <QIconEngine>
#include <QPointer>
#include <QWidget>
#include <QFileInfo>
#include <QThread>
#include <QApplication>

#include "util/color_svg_image_provider.hpp"

ColorizedSvgIcon::ColorizedSvgIcon(const QString& filename, const QColor color1, const QColor color2, const QColor accentColor, const QList<QPair<QString, QString> > &otherReplacements)
{
    QIcon& qIconRef = *this;

    const auto engine = svgIconEngine();
    if (!engine)
    {
        qWarning() << "ColorizedSvgIcon: could not create svg icon engine, icon " << filename << " will not be colorized.";
        qIconRef = QIcon(filename);
        return;
    }

    qIconRef = QIcon(engine); // QIcon takes the ownership of the engine

    QList<QPair<QString, QString>> replacements;
    {
        if (color1.isValid())
        {
            replacements.push_back({QStringLiteral(COLOR1_KEY), color1.name(QColor::HexRgb)});
            // Imported menu silhouettes also have a neutral fallback for legacy
            // QIcon consumers that do not request palette colorization.
            replacements.push_back({QStringLiteral("#747474"), color1.name(QColor::HexRgb)});
        }

        if (color2.isValid())
            replacements.push_back({QStringLiteral(COLOR2_KEY), color2.name(QColor::HexRgb)});

        if (accentColor.isValid())
            replacements.push_back({QStringLiteral(COLOR_ACCENT_KEY), accentColor.name(QColor::HexRgb)});

        replacements.append(otherReplacements.begin(), otherReplacements.end());
    }

    QByteArray data;
    {
        // Serialization (akin to `QSvgIconEngine::write()`):
        int isCompressed = 0;

        QHash<int, QString> svgFiles;
        QHash<int, QByteArray> svgBuffers;

        const QByteArray& buf = colorizeSvg(filename, replacements).first;

        const auto key = hashKey(Normal, Off); // QIcon(QString) uses these settings

        // Different colored svgs should have different file names assigned,
        // for now it is not relevant, so don't provide the file name to the
        // engine:
        // svgFiles.insert(key, filename);
        svgBuffers.insert(key, buf);

        QDataStream out(&data, QDataStream::OpenModeFlag::WriteOnly);

        out << svgFiles << isCompressed << svgBuffers;
        out << 0; // no additional added pixmaps
    }

    {
        // Feed the engine with the colorized svg content:
        QDataStream in(std::as_const(data)); // read-only
        if (!engine->read(in))
        {
            qWarning() << "ColorizedSvgIcon: svg icon engine can not read contents, icon " << filename << " will not be colorized.";
            addFile(filename);
            return;
        }
    }
}

namespace {
// Retained and dynamically created menu actions may acquire a popup-local
// palette after their icons were built. Keep the SVG source, not a raster tint:
// only styled player menus opt in; native/legacy consumers keep their original
// icon and Qt's mode/state handling unchanged.
class WidgetSvgIconEngine final : public QIconEngine
{
public:
    WidgetSvgIconEngine(const QString& fileName, const QWidget* widget)
        : m_fileName(fileName), m_widget(widget)
        , m_original(ColorizedSvgIcon(fileName, widget->palette().text().color()))
    {}

    QIconEngine* clone() const override { return new WidgetSvgIconEngine(*this); }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        icon().paint(painter, rect, Qt::AlignCenter, mode, state);
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return icon().pixmap(size, mode, state);
    }

    QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return icon().actualSize(size, mode, state);
    }

private:
    const QIcon& icon()
    {
        if (!m_widget || !m_widget->property("playerMenuWidth").isValid())
            return m_original;
        const QColor ink = m_widget->palette().text().color();
        if (ink != m_ink) {
            m_ink = ink;
            m_styled = ColorizedSvgIcon(m_fileName, ink);
        }
        return m_styled;
    }

    QString m_fileName;
    QPointer<const QWidget> m_widget;
    QIcon m_original;
    QIcon m_styled;
    QColor m_ink;
};
}

ColorizedSvgIcon ColorizedSvgIcon::colorizedIconForWidget(const QString &fileName, const QWidget *widget)
{
    assert(widget);
    return ColorizedSvgIcon(new WidgetSvgIconEngine(fileName, widget));
}

QIconEngine *ColorizedSvgIcon::svgIconEngine()
{
    static const auto plugin = []() {
        QPointer<QIconEnginePlugin> plugin;

        const auto retrieve = [&plugin]() {
#ifdef QT_STATIC
            const auto& staticPlugins = QPluginLoader::staticInstances();
            const auto it = std::find_if(staticPlugins.begin(), staticPlugins.end(), [](QObject *obj) -> bool {
                return obj->inherits("QSvgIconPlugin");
            });

            if (it != staticPlugins.end())
                plugin = qobject_cast<QIconEnginePlugin*>(*it);
#else
            QPluginLoader loader(QStringLiteral("iconengines/qsvgicon")); // Official Qt plugin
            // No need to check the metadata (or inherits `QSvgIconPlugin`), a plugin named "qsvgicon" should already support svg.
            plugin = qobject_cast<QIconEnginePlugin*>(loader.instance());
#endif
        };

        assert(qApp); // do not call before QApplication is constructed.
        if (QThread::currentThread() == qApp->thread())
            retrieve();
        else
            QMetaObject::invokeMethod(qApp, retrieve, Qt::BlockingQueuedConnection);

        return plugin;
    }();

    if (!plugin)
    {
        qWarning() << "ColorizedSvgIcon: svg icon plugin is not found.";
        return nullptr;
    }

    return plugin->create();
}
