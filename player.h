#ifndef PLAYER_H
#define PLAYER_H

#include <QGraphicsPixmapItem>
#include <QObject>

class TileMap;

class Player : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit Player(TileMap *map, QGraphicsItem *parent = nullptr);

    void move(bool up, bool down, bool left, bool right);

private:
    TileMap *tileMap;
    qreal speed;
};

#endif // PLAYER_H