#ifndef TILE_H
#define TILE_H

#include <QGraphicsPixmapItem>

class Tile : public QGraphicsPixmapItem
{
public:
    Tile(const QString &imagePath, qreal x, qreal y, QGraphicsItem *parent = nullptr);
};

#endif // TILE_H