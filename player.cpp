#include "player.h"
#include "tilemap.h"
#include <QPixmap>
#include <QDebug>

Player::Player(TileMap *map, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent), tileMap(map), speed(4.0)
{
    QPixmap pixmap(":/images/player.png");
    if (pixmap.isNull()) {
        qDebug() << "Failed to load player.png, creating blue placeholder.";
        pixmap = QPixmap(32, 32);
        pixmap.fill(Qt::blue);
    }
    setPixmap(pixmap);
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
}

void Player::move(bool up, bool down, bool left, bool right)
{
    qreal dx = 0, dy = 0;
    if (left)  dx = -speed;
    if (right) dx =  speed;
    if (up)    dy = -speed;
    if (down)  dy =  speed;

    if (dx == 0 && dy == 0) return;

    setPos(x() + dx, y());
    if (tileMap->collidesWithWall(this))
        setPos(x() - dx, y());

    setPos(x(), y() + dy);
    if (tileMap->collidesWithWall(this))
        setPos(x(), y() - dy);
}