#ifndef GAME_H
#define GAME_H

#include <QGraphicsView>
#include <QTimer>
#include <QKeyEvent>

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

private slots:
    void updateGame();   // 每帧更新玩家移动

private:
    QGraphicsScene *scene;
    Player *player;
    TileMap *tileMap;
    QTimer *gameTimer;

    // 按键状态
    bool upPressed, downPressed, leftPressed, rightPressed;
};

#endif // GAME_H