#include "game.h"
#include "player.h"
#include "tilemap.h"
#include <QDebug>
#include <QGraphicsRectItem>

Game::Game(QWidget *parent)
    : QGraphicsView(parent),
      upPressed(false), downPressed(false), leftPressed(false), rightPressed(false),
      canTeleport(true), isTeleporting(false)
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    setFixedSize(800, 600);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 加载初始地图（使用 start 点）
    loadMap(":/maps/school_map.tmj", true);
}

Game::~Game()
{
    delete tileMap;
    delete player;
}

void Game::loadMap(const QString &mapFilePath, bool useStartPoint)
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
    // 清除场景中所有已有项（瓦片、碰撞体等）
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        scene->removeItem(item);
        delete item;
    }

    // 创建新地图
    tileMap = new TileMap();
    if (!tileMap->loadFromFile(mapFilePath, scene)) {
        qDebug() << "Failed to load map:" << mapFilePath;
        // 失败回退：创建灰色背景和一个蓝色方块玩家
        QGraphicsRectItem *bg = new QGraphicsRectItem(0, 0, 800, 600);
        bg->setBrush(Qt::darkGray);
        scene->addItem(bg);
        player = new Player(nullptr);
        scene->addItem(player);
        player->setPos(100, 100);
        currentMapPath = mapFilePath;
        scene->setSceneRect(0, 0, 800, 600);
        setSceneRect(scene->sceneRect());
        centerOn(player);
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
            gameTimer->start(16);
        }
        return;
    }

    // 创建玩家
    player = new Player(tileMap);
    scene->addItem(player);

    // 根据 useStartPoint 决定初始位置
    if (useStartPoint) {
        QPointF startPos = tileMap->getPlayerStart();
        if (startPos.isNull()) {
            startPos = QPointF(100, 100);
        }
        player->setPos(startPos);
    } else {
        // 临时置零，稍后由跨地图传送逻辑覆盖位置
        player->setPos(0, 0);
    }

    currentMapPath = mapFilePath;

    // 设置场景矩形（硬编码，可改为从地图获取）
    scene->setSceneRect(0, 0, 12800, 6400);
    setSceneRect(scene->sceneRect());

    // 只有在位置有效时才进行摄像头跟随（使用 start 点时已经设置位置，跨地图传送时暂不跟随）
    if (useStartPoint) {
        centerOn(player);
    }

    // 启动游戏循环（如果尚未启动）
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
    // 仅在玩家存在时移动（防止空指针）
    if (player) {
        player->move(upPressed, downPressed, leftPressed, rightPressed);
        centerOn(player);
    }
    checkPortal();
}

void Game::checkPortal()
{
    if (!canTeleport || isTeleporting) return;
    if (!player || !tileMap) return; // 安全检查

    QRectF playerRect = player->boundingRect().translated(player->pos());
    for (const Portal &portal : tileMap->getPortals()) {
        if (playerRect.intersects(portal.rect)) {
            canTeleport = false;
            isTeleporting = true;
            // 延迟执行传送，避免在遍历中删除对象
            QTimer::singleShot(0, this, [this, portal]() {
                performTeleport(portal);
            });
            break;
        }
    }
}

void Game::performTeleport(const Portal &portal)
{
    // 二次确认玩家仍与传送门重叠（防止延迟期间玩家离开）
    if (!player || !tileMap) {
        // 意外情况，恢复标志
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    QRectF playerRect = player->boundingRect().translated(player->pos());
    bool stillIntersects = false;
    for (const Portal &p : tileMap->getPortals()) {
        if (playerRect.intersects(p.rect)) {
            stillIntersects = true;
            break;
        }
    }
    if (!stillIntersects) {
        // 玩家已离开，取消传送
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    // 判断同地图还是跨地图
    if (portal.targetMap.isEmpty() || portal.targetMap == currentMapPath) {
        // 同地图传送：直接移动玩家
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                player->setPos(p.rect.center());
                centerOn(player);
                break;
            }
        }
        // 恢复冷却
        QTimer::singleShot(2000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    } else {
        // 跨地图传送
        QString newMapPath = portal.targetMap;
        // 加载新地图，但不要自动设置 start 点
        loadMap(newMapPath, false);
        // 在新地图中查找目标传送门，设置玩家位置
        bool found = false;
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                player->setPos(p.rect.center());
                centerOn(player);
                found = true;
                break;
            }
        }
        if (!found) {
            // 如果找不到目标传送门，使用 start 点作为后备
            QPointF startPos = tileMap->getPlayerStart();
            if (!startPos.isNull()) {
                player->setPos(startPos);
                centerOn(player);
            } else {
                qDebug() << "Warning: target portal not found, and no start point.";
            }
        }
        // 跨地图冷却稍长
        QTimer::singleShot(5000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    }
}