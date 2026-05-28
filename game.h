#ifndef GAME_H
#define GAME_H

#include <QGraphicsView>
#include <QTimer>
#include <QKeyEvent>
#include <QString>
#include "maploader.h"   // 必须包含，因为使用了 Portal 结构体

class Player;
class TileMap;

class Game : public QGraphicsView
{
    Q_OBJECT
public:
    Game(QWidget *parent = nullptr);
    ~Game();

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void loadMap(const QString &mapFilePath, bool useStartPoint = true);

private slots:
    void updateGame();
    void checkPortal();
    void performTeleport(const Portal &portal); // 现在 Portal 已定义

private:
    QGraphicsScene *scene = nullptr;
    Player *player = nullptr;
    TileMap *tileMap = nullptr;
    QTimer *gameTimer = nullptr;

    bool upPressed, downPressed, leftPressed, rightPressed;
    QString currentMapPath;
    bool canTeleport;
    bool isTeleporting;
};

#endif // GAME_H