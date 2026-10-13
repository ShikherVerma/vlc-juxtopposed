// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
#include "volumecone.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

VolumeCone::VolumeCone(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    const auto repaint = [this] { update(); };
    connect(this, &VolumeCone::fillWidthChanged, this, repaint);
    connect(this, &VolumeCone::wellColorChanged, this, repaint);
    connect(this, &VolumeCone::fillColorChanged, this, repaint);
    connect(this, &VolumeCone::useGradientChanged, this, repaint);
    connect(this, &VolumeCone::darkChanged, this, repaint);
}

void VolumeCone::paint(QPainter *painter)
{
    // Preserve the original Canvas coordinates (including the well's clipped
    // right edge), gradient space, and separate translucent fill overlay.
    painter->save();
    painter->scale(width() / 88, height() / 28);
    QPainterPath well;
    well.moveTo(0, 12.6159);
    well.cubicTo(0, 10.5626, 1.5547, 8.8428, 3.5976, 8.6362);
    well.lineTo(85.6982, .3339);
    well.cubicTo(87.4654, .1552, 89, 1.5425, 89, 3.3187);
    well.lineTo(89, 24.6813);
    well.cubicTo(89, 26.4575, 87.4654, 27.8448, 85.6982, 27.6661);
    well.lineTo(3.5976, 19.3638);
    well.cubicTo(1.5547, 19.1572, 0, 17.4374, 0, 15.3841);
    well.closeSubpath();
    painter->fillPath(well, m_wellColor);

    painter->setClipRect(QRectF(0, 0, m_fillWidth, 28), Qt::IntersectClip);
    QPainterPath fill;
    fill.moveTo(1.5, 15.6984);
    fill.lineTo(1.5, 12.3016);
    fill.cubicTo(1.5, 11.2777, 2.2734, 10.4191, 3.2918, 10.3125);
    fill.lineTo(85.2918, 1.7311);
    fill.cubicTo(86.4723, 1.6076, 87.5, 2.5334, 87.5, 3.7203);
    fill.lineTo(87.5, 24.2798);
    fill.cubicTo(87.5, 25.4667, 86.4723, 26.3925, 85.2918, 26.2689);
    fill.lineTo(3.2918, 17.6875);
    fill.cubicTo(2.2734, 17.581, 1.5, 16.7224, 1.5, 15.6984);
    fill.closeSubpath();
    QLinearGradient gradient(-1, 14, 89, 14);
    gradient.setColorAt(0, QColor("#95d318"));
    gradient.setColorAt(.5788, QColor("#b2a918"));
    gradient.setColorAt(.7924, QColor("#d37918"));
    gradient.setColorAt(1, QColor("#ff3737"));
    painter->fillPath(fill, m_useGradient ? QBrush(gradient) : QBrush(m_fillColor));
    painter->fillPath(fill, m_dark ? QColor(0, 0, 0, 51) : QColor(255, 255, 255, 51));
    painter->restore();
}
