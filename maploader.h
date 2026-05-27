#ifndef MAPLOADER_H
#define MAPLOADER_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QMap>

struct Portal {
    QString id;                 // 传送门唯一标识（对应 Tiled 中对象名，如 "1-1", "5" 等）
    QRectF rect;                // 触发区域（像素坐标）
    QString targetMap;          // 目标地图文件名（资源路径，如 ":/maps/next_map.tmj"）
    QString targetPortalId;     // 目标传送门的 id
};

struct PlayerStart {
    QPointF position;
};

struct TiledMapData {
    int width;                      // 格子宽度（数量）
    int height;                     // 格子高度（数量）
    int tileWidth;                  // 单个瓦片像素宽
    int tileHeight;                 // 单个瓦片像素高
    QMap<QString, QVector<int>> layerData;    // 图层名 -> 一维 GID 数组（行优先）
    QMap<int, QString> gidToImage;            // GID -> 图片资源路径
    QVector<Portal> portals;
    PlayerStart playerStart;
};

class MapLoader
{
public:
    static bool load(const QString &jsonPath, TiledMapData &outMap);
};

#endif // MAPLOADER_H