#include "maploader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>

// Tiled 1.x 中，GID 高位用于存储翻转/旋转标志，低 29 位为实际图块 ID
static const int GID_MASK = 0x1FFFFFFF;

bool MapLoader::load(const QString &jsonPath, TiledMapData &outMap)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open map file:" << jsonPath;
        return false;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qDebug() << "Invalid JSON in map file";
        return false;
    }
    QJsonObject root = doc.object();

    // 基本尺寸
    outMap.width = root["width"].toInt();
    outMap.height = root["height"].toInt();
    outMap.tileWidth = root["tilewidth"].toInt();
    outMap.tileHeight = root["tileheight"].toInt();

    // ----- 解析瓦片集（建立 GID -> 图片路径的映射）-----
    QMap<int, QString> firstGidToImage;
    QJsonArray tilesets = root["tilesets"].toArray();
    for (const QJsonValue &tsVal : tilesets) {
        QJsonObject ts = tsVal.toObject();
        int firstGid = ts["firstgid"].toInt();
        QString imagePath;
        if (ts.contains("image")) {
            imagePath = ts["image"].toString();
            // 将绝对路径转换为 Qt 资源路径
            QString fileName = QFileInfo(imagePath).fileName();
            if (!fileName.isEmpty()) {
                imagePath = ":/images/" + fileName;
            }
        }
        firstGidToImage[firstGid] = imagePath;
    }

    // 辅助函数：根据 GID 获取图片路径（假设每个瓦片集仅一张图片）
    auto getImageForGid = [&](int gid) -> QString {
        if (gid == 0) return QString();
        int bestFirst = -1;
        for (int first : firstGidToImage.keys()) {
            if (first <= gid && first > bestFirst)
                bestFirst = first;
        }
        if (bestFirst != -1)
            return firstGidToImage[bestFirst];
        return QString();
    };

    // ----- 解析图层 -----
    QJsonArray layers = root["layers"].toArray();
    for (const QJsonValue &layerVal : layers) {
        QJsonObject layerObj = layerVal.toObject();
        QString layerName = layerObj["name"].toString();
        QString layerType = layerObj["type"].toString();

        if (layerType == "tilelayer") {
            QJsonArray dataArr = layerObj["data"].toArray();
            QVector<int> tileData;
            tileData.reserve(dataArr.size());
            for (const QJsonValue &v : dataArr) {
                int rawGid = v.toInt();
                // 清除高位标志，只保留低 29 位的实际图块 ID
                int cleanGid = rawGid & GID_MASK;
                tileData.append(cleanGid);
            }
            outMap.layerData[layerName] = tileData;

            // 为每个非 0 GID 记录图片路径
            for (int gid : tileData) {
                if (gid != 0 && !outMap.gidToImage.contains(gid)) {
                    QString img = getImageForGid(gid);
                    if (!img.isEmpty())
                        outMap.gidToImage[gid] = img;
                }
            }
        }
        else if (layerType == "objectgroup") {
            // 解析对象层（玩家起点、传送门）
            QJsonArray objects = layerObj["objects"].toArray();
            for (const QJsonValue &objVal : objects) {
                QJsonObject obj = objVal.toObject();
                QString objName = obj["name"].toString();
                double x = obj["x"].toDouble();
                double y = obj["y"].toDouble();
                double w = obj["width"].toDouble();
                double h = obj["height"].toDouble();

                // 玩家起始点（对象名精确为 "start"）
                if (objName == "start") {
                    outMap.playerStart.position = QPointF(x, y);
                }
                // 传送门：只从名为 "portals" 的对象层中读取矩形对象
                else if (layerName == "portals" && w > 0 && h > 0) {
                    Portal portal;
                    portal.id = objName;
                    portal.rect = QRectF(x, y, w, h);
                    if (obj.contains("properties")) {
                        QJsonArray props = obj["properties"].toArray();
                        for (const QJsonValue &propVal : props) {
                            QJsonObject prop = propVal.toObject();
                            QString propName = prop["name"].toString();
                            if (propName == "target_map") {
                                portal.targetMap = prop["value"].toString();
                            } else if (propName == "target_portal_id") {
                                portal.targetPortalId = prop["value"].toString();
                            }
                        }
                    }
                    outMap.portals.append(portal);
                }
            }
        }
    }

    // 确保所有非 0 GID 都有图片路径（如果没有，给个默认）
    for (auto it = outMap.gidToImage.begin(); it != outMap.gidToImage.end(); ++it) {
        if (it.value().isEmpty())
            it.value() = ":/images/floor.png";
    }

    return true;
}