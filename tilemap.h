#ifndef TILEMAP_H
#define TILEMAP_H

#include <QGraphicsScene>
#include <QVector>
#include "tile.h"
#include "maploader.h"

class TileMap
{
public:
    TileMap();
    ~TileMap();

    bool loadFromFile(const QString &jsonPath, QGraphicsScene *scene);
    void clear();

    bool collidesWithWall(QGraphicsItem *item) const;
    bool collidesWithWall(const QRectF &rect) const;
    const QVector<Portal>& getPortals() const { return portals; }
    QPointF getPlayerStart() const { return playerStart; }

private:
    QVector<Tile*> walls;
    QVector<Tile*> allTiles;
    QVector<Portal> portals;
    QPointF playerStart;
    int tileSize;
};

#endif // TILEMAP_H