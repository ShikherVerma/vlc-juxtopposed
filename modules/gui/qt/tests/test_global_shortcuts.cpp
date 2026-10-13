/* Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <QtTest/QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

class RenderControl : public QQuickRenderControl
{
public:
    explicit RenderControl(QWindow *window) : m_window(window) {}
    QWindow *renderWindow(QPoint *offset) override
    {
        if (offset)
            *offset = QPoint();
        return m_window;
    }
private:
    QWindow *m_window;
};

class TestGlobalShortcuts : public QObject
{
    Q_OBJECT
private slots:
    void offscreenWindow()
    {
        QTemporaryDir imports;
        QVERIFY(imports.isValid());
        const QString module = imports.path() + QStringLiteral("/VLC/MainInterface");
        QVERIFY(QDir().mkpath(module));
        QFile qmldir(module + QStringLiteral("/qmldir"));
        QVERIFY(qmldir.open(QIODevice::WriteOnly));
        qmldir.write("module VLC.MainInterface\nsingleton MainCtx 1.0 MainCtx.qml\n");
        qmldir.close();
        QFile singleton(module + QStringLiteral("/MainCtx.qml"));
        QVERIFY(singleton.open(QIODevice::WriteOnly));
        singleton.write("pragma Singleton\nimport QtQml\nQtObject { property bool useGlobalShortcuts: true }\n");
        singleton.close();

        QWindow native;
        native.resize(320, 240);
        native.show();
        native.requestActivate();
        QTRY_VERIFY(native.isActive());
        RenderControl control(&native);
        QQuickWindow offscreen(&control);
        QVERIFY(!offscreen.isVisible());
        QCOMPARE(QQuickRenderControl::renderWindowFor(&offscreen), &native);

        QQmlEngine engine;
        engine.addImportPath(imports.path());
        QQmlComponent component(&engine);
        // Controls installs the matcher used by the real interface. Load the
        // production ShortcutExt rather than a copy of its context binding.
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls.Basic
            import VLC.MainInterface
            import "../menus/qml" as Menus
            Item {
                property bool shortcutsEnabled: true
                onShortcutsEnabledChanged: MainCtx.useGlobalShortcuts = shortcutsEnabled
                property int shortcutContext: shortcut.context
                signal triggered()
                Menus.ShortcutExt {
                    id: shortcut
                    sequence: "Ctrl+P"
                    onActivated: triggered()
                }
            }
        )", QUrl::fromLocalFile(QStringLiteral(QT_TEST_SOURCE_DIR "/fixture.qml")));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        item->setParentItem(offscreen.contentItem());
        QSignalSpy triggered(item, SIGNAL(triggered()));
        QVERIFY(triggered.isValid());
        QCOMPARE(item->property("shortcutContext").toInt(), int(Qt::WindowShortcut));

        QTest::keyClick(&native, Qt::Key_P, Qt::ControlModifier);
        QTRY_COMPARE(triggered.count(), 1);
        item->setProperty("shortcutsEnabled", false);
        QTest::keyClick(&native, Qt::Key_P, Qt::ControlModifier);
        QCOMPARE(triggered.count(), 1);
        item->setProperty("shortcutsEnabled", true);
        QTest::keyClick(&native, Qt::Key_P, Qt::ControlModifier);
        QTRY_COMPARE(triggered.count(), 2);

        QWindow dialog;
        dialog.resize(200, 100);
        dialog.show();
        dialog.requestActivate();
        QTRY_VERIFY(dialog.isActive());
        QTest::keyClick(&dialog, Qt::Key_P, Qt::ControlModifier);
        QCOMPARE(triggered.count(), 2);
    }
};
QTEST_MAIN(TestGlobalShortcuts)
#include "test_global_shortcuts.moc"
