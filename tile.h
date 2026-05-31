#ifndef TILE_H
#define TILE_H

#include <QGraphicsPixmapItem>
#include <QSize>

class Tile : public QGraphicsPixmapItem
{
public:
    Tile(const QString &imagePath, qreal x, qreal y,
         const QSize &fixedSize = QSize(), QGraphicsItem *parent = nullptr);
};

#endif // TILE_H