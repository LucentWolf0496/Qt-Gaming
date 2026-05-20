#include "tile.h"
#include <QPixmap>

Tile::Tile(const QString &imagePath, qreal x, qreal y, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
    QPixmap pixmap(imagePath);
    // 假设所有瓦片尺寸统一为32x32，如果不是，可以缩放
    // pixmap = pixmap.scaled(32, 32);
    setPixmap(pixmap);
    setPos(x, y);
}