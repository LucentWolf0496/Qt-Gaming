#include "game.h"
#include "player.h"
#include "tilemap.h"

Game::Game(QWidget *parent)
    : QGraphicsView(parent),
      upPressed(false), downPressed(false), leftPressed(false), rightPressed(false)
{
    // 场景大小：宽1600，高1200（地图可以是任意大小，视图会跟随玩家）
    scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, 1600, 1200);
    setScene(scene);
    setFixedSize(800, 600);          // 窗口大小
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 创建瓦片地图（新手村）
    tileMap = new TileMap();
    tileMap->buildMap(scene);

    // 创建玩家
    player = new Player(tileMap);    // 传入地图指针用于碰撞检测
    scene->addItem(player);
    player->setPos(100, 100);        // 初始位置（必须是可走区域）

    // 摄像头跟随
    setSceneRect(0, 0, 1600, 1200);
    centerOn(player);

    // 游戏循环 (60 FPS)
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
    gameTimer->start(16);
}

Game::~Game()
{
    delete tileMap;
    delete player;
}

void Game::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_W: upPressed = true; break;
    case Qt::Key_S: downPressed = true; break;
    case Qt::Key_A: leftPressed = true; break;
    case Qt::Key_D: rightPressed = true; break;
    default: QGraphicsView::keyPressEvent(event);
    }
}

void Game::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_W: upPressed = false; break;
    case Qt::Key_S: downPressed = false; break;
    case Qt::Key_A: leftPressed = false; break;
    case Qt::Key_D: rightPressed = false; break;
    default: QGraphicsView::keyReleaseEvent(event);
    }
}

void Game::updateGame()
{
    // 移动玩家
    player->move(upPressed, downPressed, leftPressed, rightPressed);
    // 摄像头跟随
    centerOn(player);
}