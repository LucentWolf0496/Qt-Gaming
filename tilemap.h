#ifndef TILEMAP_H
#define TILEMAP_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QVector>

class Tile;

class TileMap
{
public:
    TileMap();
    void buildMap(QGraphicsScene *scene);   // 构建地图（添加瓦片到场景）
    bool collidesWithWall(QGraphicsItem *item) const; // 碰撞检测

private:
    QVector<Tile*> walls;   // 所有不可行走的瓦片（墙壁）
    int mapWidth, mapHeight;
    int tileSize;           // 每个瓦片的像素尺寸（这里用32x32）
};

#endif // TILEMAP_H