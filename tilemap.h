#ifndef TILEMAP_H
#define TILEMAP_H

#include <QGraphicsScene>
#include <QVector>
#include <QHash>
#include <QPair>
#include "tile.h"
#include "maploader.h"

class TileMap
{
public:
    TileMap();
    ~TileMap();

    /** 只解析地图元数据（尺寸、传送门、出生点），不再创建任何瓦片 */
    bool loadFromFile(const QString &jsonPath);
    void clear();

    /** 碰撞检测（基于空间索引） */
    bool collidesWithWall(QGraphicsItem *item) const;
    bool collidesWithWall(const QRectF &rect) const;

    /** 外部手动绘制墙壁时调用，将墙壁瓦片加入碰撞系统 */
    void addWallTile(Tile *tile);

    const QVector<Portal>& getPortals() const { return portals; }
    QPointF getPlayerStart() const { return playerStart; }

    // 地图尺寸接口
    int getMapWidth() const { return mapWidth; }
    int getMapHeight() const { return mapHeight; }
    int getTileWidth() const { return tileSize; }
    int getTileHeight() const { return tileSize; } // 瓦片为正方形

private:
    QVector<Tile*> walls;           // 所有墙壁瓦片（用于碰撞检测）
    QVector<Tile*> allTiles;        // 所有瓦片（仅用于析构清理，手动绘制时外部无需关心）
    QVector<Portal> portals;
    QPointF playerStart;
    int tileSize;
    int mapWidth = 0;
    int mapHeight = 0;

    // 空间分割（网格索引）
    static constexpr int GRID_SIZE = 8;
    QHash<QPair<int,int>, QVector<Tile*>> gridWalls;
    void buildSpatialGrid();        // 重建空间索引
    QPair<int,int> getGridCoord(int x, int y) const;
};

#endif // TILEMAP_H