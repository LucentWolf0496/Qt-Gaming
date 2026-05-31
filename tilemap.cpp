#include "tilemap.h"
#include "maploader.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QHash>
#include <QPair>

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
    gridWalls.clear();      // 清空空间索引
    portals.clear();
    playerStart = QPointF();
    mapWidth = 0;
    mapHeight = 0;
}

bool TileMap::loadFromFile(const QString &jsonPath, QGraphicsScene *scene)
{
    clear();

    TiledMapData mapData;
    if (!MapLoader::load(jsonPath, mapData)) {
        qDebug() << "Failed to parse map file:" << jsonPath;
        return false;
    }

    tileSize = mapData.tileWidth;
    // 记录地图尺寸（格子数）
    mapWidth = mapData.width;
    mapHeight = mapData.height;

    // 遍历所有瓦片图层
    for (auto it = mapData.layerData.begin(); it != mapData.layerData.end(); ++it) {
        const QString &layerName = it.key();
        const QVector<int> &data = it.value();
        int width = mapData.width;
        int height = mapData.height;
        if (data.size() != width * height) {
            qDebug() << "Layer data size mismatch for layer:" << layerName;
            continue;
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int gid = data[y * width + x];
                if (gid == 0) continue;
                QString imagePath = mapData.gidToImage.value(gid, "");
                if (imagePath.isEmpty()) continue;

                // 随机替换 floor / wall 图片，并固定缩放为瓦片大小
                QString finalPath = imagePath;
                QSize fixedSize;
                if (imagePath.endsWith("floor.png")) {
                    finalPath = (QRandomGenerator::global()->bounded(2) == 0)
                                ? ":/images/floor_dark.png" : ":/images/floor_light.png";
                    fixedSize = QSize(tileSize, tileSize);
                } else if (imagePath.endsWith("wall.png")) {
                    int r = QRandomGenerator::global()->bounded(3);
                    finalPath = QString(":/images/wall_%1.png").arg(r + 1);
                    fixedSize = QSize(tileSize, tileSize);
                }

                Tile *tile = new Tile(finalPath, x * tileSize, y * tileSize, fixedSize);
                scene->addItem(tile);
                allTiles.append(tile);

                // 碰撞检测：仅图层名为 "wall" 的瓦片加入墙壁列表
                if (layerName == "wall") {
                    walls.append(tile);
                }
            }
        }
    }

    // 保存传送门和玩家出生点
    portals = mapData.portals;
    playerStart = mapData.playerStart.position;

    // ========== 构建空间分割索引 ==========
    buildSpatialGrid();

    return true;
}

void TileMap::buildSpatialGrid()
{
    gridWalls.clear();
    int gridPixelSize = tileSize * GRID_SIZE;  // 每个网格的像素大小

    for (Tile *wall : walls) {
        // 获取墙壁瓦片左上角的世界坐标
        qreal wx = wall->x();
        qreal wy = wall->y();
        // 计算所在网格坐标
        int gridX = static_cast<int>(wx) / gridPixelSize;
        int gridY = static_cast<int>(wy) / gridPixelSize;
        gridWalls[{gridX, gridY}].append(wall);
    }
    qDebug() << "[SpatialGrid] Built with" << gridWalls.size() << "grid cells.";
}

QPair<int,int> TileMap::getGridCoord(int x, int y) const
{
    int gridPixelSize = tileSize * GRID_SIZE;
    return { x / gridPixelSize, y / gridPixelSize };
}

bool TileMap::collidesWithWall(QGraphicsItem *item) const
{
    if (!item) return false;
    // 复用矩形碰撞检测，避免重复代码
    return collidesWithWall(item->sceneBoundingRect());
}

bool TileMap::collidesWithWall(const QRectF &rect) const
{
    if (walls.isEmpty()) return false;

    int gridPixelSize = tileSize * GRID_SIZE;
    // 计算 rect 覆盖的网格范围
    int minGridX = static_cast<int>(rect.left()) / gridPixelSize;
    int maxGridX = static_cast<int>(rect.right()) / gridPixelSize;
    int minGridY = static_cast<int>(rect.top()) / gridPixelSize;
    int maxGridY = static_cast<int>(rect.bottom()) / gridPixelSize;

    // 只遍历这些网格内的墙壁
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