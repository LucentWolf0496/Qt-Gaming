// tilemap.h
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

    bool loadFromFile(const QString &jsonPath);
    void clear();

    // 墙壁碰撞
    bool collidesWithWall(QGraphicsItem *item) const;
    bool collidesWithWall(const QRectF &rect) const;
    void addWallTile(Tile *tile);
    void removeWallTile(Tile *tile);

    // ========== 新增：水碰撞（仅玩家） ==========
    void addWaterTile(Tile *tile);                       // 添加水瓦片到空间索引
    void addWaterTiles(const QVector<Tile*> &tiles); // 批量添加（推荐）
    void buildWaterGrid();                         // 改为 public，以便外部手动重建
    bool collidesWithWater(const QRectF &rect) const;    // 检测是否与水碰撞

    const QVector<Portal>& getPortals() const { return portals; }
    QPointF getPlayerStart() const { return playerStart; }

    int getMapWidth() const { return mapWidth; }
    int getMapHeight() const { return mapHeight; }
    int getTileWidth() const { return tileSize; }
    int getTileHeight() const { return tileSize; }

    /** 添加火焰区域矩形到空间索引 */
    void addFireRect(const QRectF &rect);
    /** 检测矩形是否与任何火焰区域相交（使用空间索引） */
    bool collidesWithFire(const QRectF &rect) const;

private:
    QVector<Tile*> walls;
    QVector<Tile*> allTiles;
    QVector<Portal> portals;
    QPointF playerStart;
    int tileSize;
    int mapWidth = 0;
    int mapHeight = 0;

    // 墙壁空间索引
    static constexpr int GRID_SIZE = 8;
    QHash<QPair<int,int>, QVector<Tile*>> gridWalls;
    void buildSpatialGrid();
    QPair<int,int> getGridCoord(int x, int y) const;

    // ========== 新增：水空间索引 ==========
    QVector<Tile*> waterTiles;                           // 所有水瓦片
    QHash<QPair<int,int>, QVector<Tile*>> gridWaters;    // 水的网格索引

    // 火焰空间索引（与墙壁结构类似）
    QHash<QPair<int,int>, QVector<QRectF>> gridFires;
    void buildFireGrid();   // 重建火焰网格（在添加完成后调用）
    int m_fireGridPixelSize = 0;  // 缓存网格像素大小
};

#endif // TILEMAP_H