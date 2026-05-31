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
    portals.clear();
    playerStart = QPointF();
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

    return true;
}

bool TileMap::collidesWithWall(QGraphicsItem *item) const
{
    for (Tile *wall : walls) {
        if (item->collidesWithItem(wall))
            return true;
    }
    return false;
}

bool TileMap::collidesWithWall(const QRectF &rect) const
{
    for (Tile *wall : walls) {
        if (wall->sceneBoundingRect().intersects(rect))
            return true;
    }
    return false;
}