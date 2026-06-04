// tilemap.cpp
#include "tilemap.h"
#include "maploader.h"
#include <QDebug>
#include <QRandomGenerator>

TileMap::TileMap() : tileSize(32) {}

TileMap::~TileMap()
{
    clear();
}

void TileMap::clear()
{
    for (Tile *t : allTiles) {
        if (t && t->scene()) t->scene()->removeItem(t);
        delete t;
    }
    allTiles.clear();
    walls.clear();
    gridWalls.clear();

    // 清理水相关
    waterTiles.clear();
    gridWaters.clear();

    portals.clear();
    playerStart = QPointF();
    mapWidth = 0;
    mapHeight = 0;

    gridFires.clear();
    m_fireGridPixelSize = 0;
}

bool TileMap::loadFromFile(const QString &jsonPath)
{
    clear();

    TiledMapData mapData;
    if (!MapLoader::load(jsonPath, mapData)) {
        qDebug() << "Failed to parse map file:" << jsonPath;
        return false;
    }

    tileSize = mapData.tileWidth;
    mapWidth = mapData.width;
    mapHeight = mapData.height;

    portals = mapData.portals;
    playerStart = mapData.playerStart.position;

    // 注意：瓦片由 Game 类手动绘制，本函数只保存元数据
    return true;
}

void TileMap::addWallTile(Tile *tile)
{
    if (!tile) return;
    walls.append(tile);
    allTiles.append(tile);
    buildSpatialGrid();
}

void TileMap::buildSpatialGrid()
{
    gridWalls.clear();
    int gridPixelSize = tileSize * GRID_SIZE;
    for (Tile *wall : walls) {
        qreal wx = wall->x();
        qreal wy = wall->y();
        int gridX = static_cast<int>(wx) / gridPixelSize;
        int gridY = static_cast<int>(wy) / gridPixelSize;
        gridWalls[{gridX, gridY}].append(wall);
    }
}

QPair<int,int> TileMap::getGridCoord(int x, int y) const
{
    int gridPixelSize = tileSize * GRID_SIZE;
    return { x / gridPixelSize, y / gridPixelSize };
}

bool TileMap::collidesWithWall(QGraphicsItem *item) const
{
    if (!item) return false;
    return collidesWithWall(item->sceneBoundingRect());
}

bool TileMap::collidesWithWall(const QRectF &rect) const
{
    if (walls.isEmpty()) return false;
    int gridPixelSize = tileSize * GRID_SIZE;
    int minGridX = static_cast<int>(rect.left()) / gridPixelSize;
    int maxGridX = static_cast<int>(rect.right()) / gridPixelSize;
    int minGridY = static_cast<int>(rect.top()) / gridPixelSize;
    int maxGridY = static_cast<int>(rect.bottom()) / gridPixelSize;

    for (int gx = minGridX; gx <= maxGridX; ++gx) {
        for (int gy = minGridY; gy <= maxGridY; ++gy) {
            auto it = gridWalls.find({gx, gy});
            if (it != gridWalls.end()) {
                for (Tile *wall : it.value()) {
                    if (wall->sceneBoundingRect().intersects(rect))
                        return true;
                }
            }
        }
    }
    return false;
}

// ========== 水的实现 ==========

void TileMap::addWaterTile(Tile *tile)
{
    if (!tile) return;
    waterTiles.append(tile);
    allTiles.append(tile);
    buildWaterGrid();
}

void TileMap::buildWaterGrid()
{
    gridWaters.clear();
    int gridPixelSize = tileSize * GRID_SIZE;
    for (Tile *water : waterTiles) {
        qreal wx = water->x();
        qreal wy = water->y();
        int gridX = static_cast<int>(wx) / gridPixelSize;
        int gridY = static_cast<int>(wy) / gridPixelSize;
        gridWaters[{gridX, gridY}].append(water);
    }
}

void TileMap::addWaterTiles(const QVector<Tile*> &tiles)
{
    if (tiles.isEmpty()) return;
    waterTiles.append(tiles);
    allTiles.append(tiles);
    buildWaterGrid();   // 一次性重建
}

bool TileMap::collidesWithWater(const QRectF &rect) const
{
    if (waterTiles.isEmpty()) return false;
    int gridPixelSize = tileSize * GRID_SIZE;
    int minGridX = static_cast<int>(rect.left()) / gridPixelSize;
    int maxGridX = static_cast<int>(rect.right()) / gridPixelSize;
    int minGridY = static_cast<int>(rect.top()) / gridPixelSize;
    int maxGridY = static_cast<int>(rect.bottom()) / gridPixelSize;

    for (int gx = minGridX; gx <= maxGridX; ++gx) {
        for (int gy = minGridY; gy <= maxGridY; ++gy) {
            auto it = gridWaters.find({gx, gy});
            if (it != gridWaters.end()) {
                for (Tile *water : it.value()) {
                    if (water->sceneBoundingRect().intersects(rect))
                        return true;
                }
            }
        }
    }
    return false;
}

void TileMap::removeWallTile(Tile *tile)
{
    if (!tile) return;
    // 从墙壁列表中移除
    walls.removeAll(tile);
    // 同时从 allTiles 中移除（可选，便于统一清理，但并非必须）
    allTiles.removeAll(tile);
    // 重建空间索引（因为墙壁数量少，重建开销可接受）
    buildSpatialGrid();
}

// ========== 在 tilemap.cpp 中添加 ==========

void TileMap::addFireRect(const QRectF &rect)
{
    if (m_fireGridPixelSize == 0) {
        m_fireGridPixelSize = tileSize * GRID_SIZE;  // 复用 GRID_SIZE = 8
    }
    int gridX = static_cast<int>(rect.x()) / m_fireGridPixelSize;
    int gridY = static_cast<int>(rect.y()) / m_fireGridPixelSize;
    gridFires[{gridX, gridY}].append(rect);
}

void TileMap::buildFireGrid()
{
    // 本实现中不需要单独重建，因为添加时已经按网格存储。
    // 但如果需要批量重建（例如清除后重新添加），可以遍历 fireRects 并调用 addFireRect。
    // 目前我们只在添加每个火焰矩形时直接插入网格，所以 buildFireGrid 可以为空或保留以备后用。
}

bool TileMap::collidesWithFire(const QRectF &rect) const
{
    if (gridFires.isEmpty()) return false;
    int gridPixelSize = tileSize * GRID_SIZE;
    int minGridX = static_cast<int>(rect.left()) / gridPixelSize;
    int maxGridX = static_cast<int>(rect.right()) / gridPixelSize;
    int minGridY = static_cast<int>(rect.top()) / gridPixelSize;
    int maxGridY = static_cast<int>(rect.bottom()) / gridPixelSize;

    for (int gx = minGridX; gx <= maxGridX; ++gx) {
        for (int gy = minGridY; gy <= maxGridY; ++gy) {
            auto it = gridFires.find({gx, gy});
            if (it != gridFires.end()) {
                for (const QRectF &fireRect : it.value()) {
                    if (fireRect.intersects(rect))
                        return true;
                }
            }
        }
    }
    return false;
}