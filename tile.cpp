#include "tile.h"
#include <QPixmap>
#include <QDebug>

Tile::Tile(const QString &imagePath, qreal x, qreal y, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        qDebug() << "Failed to load tile image:" << imagePath;
    } else {
        setPixmap(pixmap);
    }
    setPos(x, y);
}