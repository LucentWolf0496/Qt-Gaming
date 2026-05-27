#include "game.h"
#include "player.h"
#include "tilemap.h"
#include <QDebug>

Game::Game(QWidget *parent)
    : QGraphicsView(parent),
      upPressed(false), downPressed(false), leftPressed(false), rightPressed(false),
      canTeleport(true)   // 添加这一行
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    setFixedSize(800, 600);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 加载地图
    loadMap(":/maps/school_map.tmj");
}

Game::~Game()
{
    delete tileMap;
    delete player;
}

void Game::loadMap(const QString &mapFilePath)
{
    // 清理旧地图
    if (tileMap) {
        delete tileMap;
        tileMap = nullptr;
    }
    if (player) {
        if (player->scene()) scene->removeItem(player);
        delete player;
        player = nullptr;
    }
    // 清除场景中所有已有项
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        scene->removeItem(item);
        delete item;
    }

    // 创建新地图
    tileMap = new TileMap();
    if (!tileMap->loadFromFile(mapFilePath, scene)) {
        qDebug() << "Failed to load map:" << mapFilePath;
        // 不再添加黑屏回退，直接返回，程序可能看不到任何东西但不会崩溃
        return;
    }

    // 创建玩家
    player = new Player(tileMap);
    scene->addItem(player);
    QPointF startPos = tileMap->getPlayerStart();
    if (startPos.isNull()) {
        startPos = QPointF(100, 100);
    }
    player->setPos(startPos);

    currentMapPath = mapFilePath;

    // 设置场景矩形（这里硬编码，实际应从地图数据获取）
    scene->setSceneRect(0, 0, 6400, 3200);
    setSceneRect(scene->sceneRect());
    centerOn(player);

    // 启动游戏循环
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
        gameTimer->start(16);
    }
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
    player->move(upPressed, downPressed, leftPressed, rightPressed);
    centerOn(player);
    // 如果不想保留传送门，可以删除下面这行
    checkPortal();
}

void Game::checkPortal()
{
    // 传送门逻辑（如果你不需要，可以留空函数体或删除调用）
    // 这里保留一个简单版本，无冷却
    QRectF playerRect = player->boundingRect().translated(player->pos());
    for (const Portal &portal : tileMap->getPortals()) {
        if (playerRect.intersects(portal.rect)) {
            if (portal.targetMap.isEmpty() || portal.targetMap == currentMapPath) {
                // 同地图传送
                for (const Portal &p : tileMap->getPortals()) {
                    if (p.id == portal.targetPortalId) {
                        player->setPos(p.rect.center());
                        centerOn(player);
                        break;
                    }
                }
            } else {
                // 跨地图传送
                loadMap(portal.targetMap);
                for (const Portal &p : tileMap->getPortals()) {
                    if (p.id == portal.targetPortalId) {
                        player->setPos(p.rect.center());
                        centerOn(player);
                        break;
                    }
                }
            }
            break; // 只触发第一个碰撞的传送门
        }
    }
}