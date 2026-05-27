#ifndef GAME_H
#define GAME_H

#include <QGraphicsView>
#include <QTimer>
#include <QKeyEvent>
#include <QString>

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
    void loadMap(const QString &mapFilePath);   // 切换地图

private slots:
    void updateGame();   // 每帧更新玩家移动
    void checkPortal();  // 检测传送门触发

private:
    QGraphicsScene *scene = nullptr;
    Player *player = nullptr;
    TileMap *tileMap = nullptr;
    QTimer *gameTimer = nullptr;

    bool upPressed, downPressed, leftPressed, rightPressed;
    QString currentMapPath;
    bool canTeleport;           // 传送冷却标志
};

#endif // GAME_H