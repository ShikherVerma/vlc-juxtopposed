// Regression probe for retained/dynamic widget SVG icons. Run against the real
// colorizedsvgicon.cpp and color_svg_image_provider.cpp (round2/icon-test.sh).
#include "util/colorizedsvgicon.hpp"
#include <QApplication>
#include <QMenu>
#include <QPalette>
#include <QTemporaryFile>
#include <QImage>
#include <QDebug>

static void require(bool condition, const char* message)
{
    if (!condition)
        qFatal("%s", message);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryFile svg;
    require(svg.open(), "temporary SVG");
    svg.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\"><rect width=\"24\" height=\"24\" fill=\"#FF00FF\"/></svg>");
    svg.flush();
    auto image = [](const QIcon& icon, QIcon::Mode mode, QIcon::State state) {
        return icon.pixmap(QSize(24, 24), mode, state).toImage();
    };
    QMenu menu;
    QPalette palette = menu.palette();
    palette.setColor(QPalette::Text, Qt::black);
    menu.setPalette(palette);
    const QIcon retained = ColorizedSvgIcon::colorizedIconForWidget(svg.fileName(), &menu);
    const QIcon legacy = ColorizedSvgIcon(svg.fileName(), Qt::black);
    for (auto mode : {QIcon::Normal, QIcon::Selected, QIcon::Disabled})
        for (auto state : {QIcon::Off, QIcon::On})
            require(image(retained, mode, state) == image(legacy, mode, state), "legacy mode/state changed");

    menu.setProperty("playerMenuWidth", 265);
    for (const auto ink : {QColor("#8f8f8f"), QColor("#2e2e2e"), QColor("#8f8f8f")}) {
        palette.setColor(QPalette::Text, ink);
        menu.setPalette(palette);
        const QIcon expected = ColorizedSvgIcon(svg.fileName(), ink);
        const QIcon dynamic = ColorizedSvgIcon::colorizedIconForWidget(svg.fileName(), &menu);
        for (auto mode : {QIcon::Normal, QIcon::Selected, QIcon::Disabled})
            for (auto state : {QIcon::Off, QIcon::On}) {
                require(image(retained, mode, state) == image(expected, mode, state), "retained mode/state mismatch");
                require(image(dynamic, mode, state) == image(expected, mode, state), "dynamic mode/state mismatch");
            }
        require(image(retained, QIcon::Normal, QIcon::Off).pixelColor(12, 12) == ink, "authored ink mismatch");
        qInfo() << "retained/dynamic and selected/disabled/on/off pass:" << ink.name();
    }
    menu.setProperty("playerMenuWidth", QVariant());
    require(image(retained, QIcon::Normal, QIcon::Off) == image(legacy, QIcon::Normal, QIcon::Off), "legacy opt-out changed");
    QIcon orphan;
    {
        QMenu transient;
        orphan = ColorizedSvgIcon::colorizedIconForWidget(svg.fileName(), &transient);
    }
    require(!image(orphan, QIcon::Normal, QIcon::Off).isNull(), "destroyed-widget fallback");
    qInfo() << "legacy opt-out and destroyed-widget fallback pass";
}
