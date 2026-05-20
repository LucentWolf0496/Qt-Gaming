#include "player.h"
#include "tilemap.h"
#include <QPixmap>

Player::Player(TileMap *map, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent), tileMap(map), speed(4.0)
{
    // 加载图片
    QPixmap pixmap(":/resources/images/player.png");
    // 缩放图片到合适大小（如果原图不是32x32）
    // pixmap = pixmap.scaled(32, 32);
    setPixmap(pixmap);
    // 设置碰撞检测形状为矩形（默认就是 boundingRect，可省略）
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