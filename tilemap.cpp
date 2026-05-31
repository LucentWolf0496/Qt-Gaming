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
    portals.clear();
    playerStart = QPointF();
    mapWidth = 0;
    mapHeight = 0;
}

bool TileMap::loadFromFile(const QString &jsonPath)
{
    clear();

    TiledMapData mapData;
    if (!MapLoader::load(jsonPath, mapData)) {
        qDebug() << "Failed to parse map file:" << jsonPath;
        return false;
    }

    // 只保存地图基本尺寸和瓦片大小
    tileSize = mapData.tileWidth;
    mapWidth = mapData.width;
    mapHeight = mapData.height;

    // 保存传送门和玩家出生点（从对象层解析）
    portals = mapData.portals;
    playerStart = mapData.playerStart.position;

    // 注意：这里不再创建任何瓦片！
    // 所有瓦片（包括 floor, wall, 装饰）都由 Game 类手动绘制

    return true;
}

void TileMap::addWallTile(Tile *tile)
{
    if (!tile) return;
    walls.append(tile);
    allTiles.append(tile);      // 加入 allTiles 以便统一清理
    // 增量添加到空间索引（简单起见，重建整个索引）
    // 或者为了性能可以只添加当前墙壁，但重建开销不大（墙壁数量有限）
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
    // qDebug() << "[SpatialGrid] Built with" << gridWalls.size() << "grid cells.";
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