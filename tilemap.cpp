#include "tilemap.h"
#include "tile.h"
#include <QGraphicsScene>

TileMap::TileMap() : tileSize(32)
{
}

void TileMap::buildMap(QGraphicsScene *scene)
{
    // 定义地图尺寸（格子数）
    mapWidth = 40;   // 40 * 32 = 1280 像素宽
    mapHeight = 30;  // 30 * 32 = 960 像素高

    // 定义地图数据（0=空地，1=墙壁）
    // 这是一个简单的矩形边框 + 内部几个障碍物，模拟“校史馆”房间
    int mapData[30][40] = {0};  // 先全0，再赋值

    // 1. 四周墙壁
    for (int x = 0; x < mapWidth; ++x) {
        mapData[0][x] = 1;                 // 上边界
        mapData[mapHeight-1][x] = 1;       // 下边界
    }
    for (int y = 0; y < mapHeight; ++y) {
        mapData[y][0] = 1;                 // 左边界
        mapData[y][mapWidth-1] = 1;        // 右边界
    }

    // 2. 内部添加一些柱子/展柜（墙壁）
    // 一个展柜位于 (10,10) 到 (12,12) 区域
    for (int x = 10; x <= 12; ++x)
        for (int y = 10; y <= 12; ++y)
            mapData[y][x] = 1;

    // 另一个展柜在 (25,20) 附近
    mapData[20][25] = 1;
    mapData[20][26] = 1;
    mapData[21][25] = 1;
    mapData[21][26] = 1;

    // 一个中心雕像（不可通过）
    mapData[15][20] = 1;

    // 遍历地图
    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            qreal px = x * tileSize;
            qreal py = y * tileSize;

            if (mapData[y][x] == 1) {  // 墙壁
                Tile *tile = new Tile(":/resources/images/wall.png", px, py);
                scene->addItem(tile);
                walls.append(tile);    // 仍然添加到碰撞列表
            } else {                   // 地板
                Tile *floor = new Tile(":/resources/images/floor.png", px, py);
                scene->addItem(floor);
                // 地板不加入 walls，所以玩家可以走上去
            }
        }
    }

    // 添加传送门（也是墙壁类，不可走）
    Tile *portal = new Tile(":/resources/images/portal.png", 35 * tileSize, 15 * tileSize);
    scene->addItem(portal);
    walls.append(portal);
}

bool TileMap::collidesWithWall(QGraphicsItem *item) const
{
    for (Tile *wall : walls) {
        if (item->collidesWithItem(wall))
            return true;
    }
    return false;
}