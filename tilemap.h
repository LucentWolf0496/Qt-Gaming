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

    // 新增：获取地图尺寸（格子数）和瓦片尺寸
    int getMapWidth() const { return mapWidth; }
    int getMapHeight() const { return mapHeight; }
    int getTileWidth() const { return tileSize; }
    int getTileHeight() const { return tileSize; } // 假设正方形瓦片

private:
    QVector<Tile*> walls;
    QVector<Tile*> allTiles;
    QVector<Portal> portals;
    QPointF playerStart;
    int tileSize;
    int mapWidth = 0;   // 新增：地图宽度（格子数）
    int mapHeight = 0;  // 新增：地图高度（格子数）
};

#endif // TILEMAP_H