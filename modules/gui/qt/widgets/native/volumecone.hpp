// Copyright (C) 2026 VideoLAN; GPL-2.0-or-later
#ifndef VLC_VOLUME_CONE_HPP
#define VLC_VOLUME_CONE_HPP

#include <QQuickPaintedItem>
#include <QColor>

// QPainter-backed equivalent of the authored cone. Qt contrib deliberately
// disables Quick Canvas and Paths/Shapes; QQuickPaintedItem remains available.
class VolumeCone : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal fillWidth MEMBER m_fillWidth NOTIFY fillWidthChanged FINAL)
    Q_PROPERTY(QColor wellColor MEMBER m_wellColor NOTIFY wellColorChanged FINAL)
    Q_PROPERTY(QColor fillColor MEMBER m_fillColor NOTIFY fillColorChanged FINAL)
    Q_PROPERTY(bool useGradient MEMBER m_useGradient NOTIFY useGradientChanged FINAL)
    Q_PROPERTY(bool dark MEMBER m_dark NOTIFY darkChanged FINAL)

public:
    explicit VolumeCone(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

signals:
    void fillWidthChanged();
    void wellColorChanged();
    void fillColorChanged();
    void useGradientChanged();
    void darkChanged();

private:
    qreal m_fillWidth = 0;
    QColor m_wellColor = Qt::transparent;
    QColor m_fillColor = Qt::transparent;
    bool m_useGradient = true;
    bool m_dark = true;
};

#endif
