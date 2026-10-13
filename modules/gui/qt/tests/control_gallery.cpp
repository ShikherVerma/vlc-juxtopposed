// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
// Standalone visual harness: real QML, palette, ColorContext, icons and navigation.
#include <QApplication>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>
#include <QPainter>
#include "style/systempalette.hpp"
#include "style/vlcicons.hpp"
#include "widgets/native/navigation_attached.hpp"

extern "C" const char vlc_module_name[] = "control_gallery";

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 5) return 2;
    const QString root = argv[1];
    for (const auto &face : {"Light", "Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(root + "/vlc/modules/gui/qt/fonts/Inter-" + face + ".ttf");
    app.setFont(QFont("Inter", 10));
    qmlRegisterType<SystemPalette>("VLC.Style", 1, 0, "SystemPalette");
    qmlRegisterType<ColorContext>("VLC.Style", 1, 0, "ColorContext");
    qmlRegisterSingletonType<VLCIcons>("VLC.Style", 1, 0, "VLCIcons", [](QQmlEngine *, QJSEngine *) -> QObject * { return new VLCIcons; });
    qmlRegisterUncreatableMetaObject(ColorSchemeModel::staticMetaObject, "VLC.Style", 1, 0, "ColorSchemeModel", "enum");
    qmlRegisterUncreatableType<QWindow>("VLC.MainInterface", 1, 0, "QtWindow", "window");
    qmlRegisterUncreatableType<NavigationAttached>("VLC.MainInterface", 1, 0, "Navigation", "attached");
    QQmlApplicationEngine engine;
    engine.addImportPath(argv[2]);
    engine.rootContext()->setContextProperty("galleryDark", QString(argv[3]) == "dark");
    engine.load(QUrl::fromLocalFile(root + "/vlc/modules/gui/qt/tests/ControlGallery.qml"));
    if (engine.rootObjects().isEmpty()) return 3;
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QTimer::singleShot(1800, [&] {
        QImage atlas = window->grabWindow();
        // A window has only one pointer/focus target. Stitch actual pointer,
        // held-press and keyboard-focus cells from sequential QtTest renders.
        for (int row = 0; row < 21; ++row) {
            for (int state = 1; state <= 3; ++state) {
                const QRect cell(200 + 370 * state, 64 + 68 * row, 370, 64);
                const QPoint point(cell.x() + 12, cell.y() + 34);
                QTest::mouseMove(window, state == 3 ? QPoint(2400, 1600) : point);
                if (state == 2) QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, point);
                if (state == 3) window->setProperty("focusRow", row);
                // Search's real popup returns focus to its opener after its
                // exit animation; let it finish before assigning Tab focus.
                if (state == 3 && row == 15) {
                    QTest::qWait(650);
                    window->setProperty("focusRow", -1);
                    window->setProperty("focusRow", row);
                }
                QTest::qWait(250);
                const QImage frame = window->grabWindow();
                QPainter painter(&atlas);
                painter.drawImage(cell, frame, cell);
                if (state == 2) QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, point);
            }
        }
        if (!atlas.save(argv[4])) { app.exit(4); return; }
        window->setProperty("focusRow", 4);
        QTest::qWait(50);
        auto toggle = window->activeFocusItem();
        if (!toggle) { app.exit(5); return; }
        const bool before = toggle->property("checked").toBool();
        QTest::keyClick(window, Qt::Key_Space);
        if (toggle->property("checked").toBool() == before) { app.exit(6); return; }
        qInfo("PASS keyboard Space toggles focused CheckButton; actual focused cells captured.");
        window->setProperty("focusRow", 6);
        QTest::qWait(50);
        auto combo = window->activeFocusItem();
        auto popup = QQmlProperty::read(combo, "popup").value<QObject *>();
        if (!popup || !QMetaObject::invokeMethod(popup, "open")) { app.exit(7); return; }
        QTest::qWait(600);
        QString popupPath = argv[4];
        popupPath.replace("controls-", "dropdown-");
        if (!window->grabWindow().save(popupPath)) { app.exit(8); return; }
        QTest::keyClick(window, Qt::Key_Down);
        QTest::keyClick(window, Qt::Key_Return);
        if (combo->property("currentIndex").toInt() != 1) { app.exit(9); return; }
        qInfo("PASS keyboard Down/Return selects second ComboBox option.");
        app.quit();
    });
    return app.exec();
}
