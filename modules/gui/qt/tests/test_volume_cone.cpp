// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
#include <QtTest/QtTest>
#include <QPainter>
#include "widgets/native/volumecone.hpp"

class TestVolumeCone : public QObject
{
    Q_OBJECT
private slots:
    void levels_data()
    {
        QTest::addColumn<int>("fillWidth");
        QTest::addColumn<bool>("dark");
        QTest::addColumn<int>("scale");
        for (bool dark : {false, true})
            for (int scale : {1, 2})
                for (int width : {1, 33, 63, 75, 88})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(dark).arg(scale).arg(width)))
                        << width << dark << scale;
    }

    void levels()
    {
        QFETCH(int, fillWidth);
        QFETCH(bool, dark);
        QFETCH(int, scale);
        VolumeCone cone;
        cone.setWidth(88 * scale);
        cone.setHeight(28 * scale);
        const QColor well("#282828");
        QVERIFY(cone.setProperty("wellColor", well));
        QVERIFY(cone.setProperty("fillWidth", fillWidth));
        QVERIFY(cone.setProperty("dark", dark));
        QImage image(88 * scale, 28 * scale, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, cone.antialiasing());
        cone.paint(&painter);
        painter.end();
        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(image.pixelColor(0, 14 * scale), well);
        if (fillWidth < 88)
            QCOMPARE(image.pixelColor((fillWidth + 1) * scale, 14 * scale), well);
        if (fillWidth > 1)
        {
            const QColor green = image.pixelColor(5 * scale, 14 * scale);
            QVERIFY(green.green() > green.red());
            QVERIFY(green.red() > green.blue());
        }
        if (fillWidth >= 75)
        {
            const QColor boosted = image.pixelColor(73 * scale, 14 * scale);
            QVERIFY(boosted.red() > boosted.green());
        }
    }

    void propertiesAndDisabledFill()
    {
        VolumeCone cone;
        cone.setWidth(88);
        cone.setHeight(28);
        QSignalSpy changed(&cone, &VolumeCone::fillWidthChanged);
        QVERIFY(cone.setProperty("fillWidth", 63.));
        QCOMPARE(changed.count(), 1);
        QVERIFY(cone.setProperty("fillWidth", 63.));
        QCOMPARE(changed.count(), 1);
        QVERIFY(cone.setProperty("fillWidth", 75.));
        QCOMPARE(changed.count(), 2);
        QVERIFY(cone.setProperty("useGradient", false));
        QVERIFY(cone.setProperty("fillColor", QColor("#808080")));
        QVERIFY(cone.setProperty("dark", true));
        QImage image(88, 28, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        cone.paint(&painter);
        painter.end();
        QCOMPARE(image.pixelColor(20, 14), QColor("#666666"));
    }
};
QTEST_MAIN(TestVolumeCone)
#include "test_volume_cone.moc"
