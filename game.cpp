#include "game.h"
#include "player.h"
#include "tilemap.h"
#include "spawner.h"
#include "pet.h"
#include <QDebug>
#include <QGraphicsRectItem>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QtMath>  // qSqrt
#include <QImageReader>

namespace {
    QVector<QPixmap> g_bombFrames;
    bool g_bombLoaded = false;
    QVector<QPixmap> g_fireBgFrames;
    bool g_fireBgLoaded = false;
}

QVector<QPixmap> g_daolangFrames;
bool g_daolangLoaded = false;

namespace {

    void loadBombFrames()
    {
        if (g_bombLoaded) return;
        g_bombLoaded = true;
        QImageReader reader(":/images/bomb.gif");
        reader.setAutoDetectImageFormat(true);
        int count = 0;
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                // 每 2 帧取 1 帧，减少总帧数
                if (count % 2 == 0) {
                    g_bombFrames.append(QPixmap::fromImage(img).scaled(
                        96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                count++;
            }
        }
        if (g_bombFrames.isEmpty()) {
            qDebug() << "Failed to load bomb.gif frames";
        }
    }

    void loadFireBgFrames()
    {
        if (g_fireBgLoaded) return;
        g_fireBgLoaded = true;
        QImageReader reader(":/images/player_background_fire.gif");
        reader.setAutoDetectImageFormat(true);
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                g_fireBgFrames.append(QPixmap::fromImage(img).scaled(
                    80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (g_fireBgFrames.isEmpty()) {
            qDebug() << "Failed to load player_background_fire.gif frames";
        }
    }

    void loadDaolangFrames()
    {
        if (g_daolangLoaded) return;
        g_daolangLoaded = true;
        QImageReader reader(":/images/daolang_left.gif");
        reader.setAutoDetectImageFormat(true);
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                g_daolangFrames.append(QPixmap::fromImage(img).scaled(
                    80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (g_daolangFrames.isEmpty()) {
            qDebug() << "Failed to load daolang_left.gif frames";
        }
    }
}

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

    // 预加载 Projectile / Bomb / FireBg / Daolang 帧缓存，避免首次释放技能时卡顿
    preloadProjectileFrames();
    loadBombFrames();
    loadFireBgFrames();
    loadDaolangFrames();

    // 加载初始地图（使用 start 点）
    loadMap(":/maps/school_map.tmj", true);
}

Game::~Game()
{
    // 清理所有活跃的流星粒子（避免内存泄漏）
    for (Projectile *p : projectiles) {
        delete p;
    }
    projectiles.clear();

    // 清理所有简易子弹
    for (SimpleProjectile *sp : simpleProjectiles) {
        delete sp;
    }
    simpleProjectiles.clear();

    // 清理所有冰魄八荒子弹
    for (BlueProjectile *bp : blueProjectiles) {
        delete bp;
    }
    blueProjectiles.clear();

    // 清理所有破空梭
    for (TriangleProjectile *tp : triangleProjectiles) {
        delete tp;
    }
    triangleProjectiles.clear();

    // 清理所有刀浪
    for (BladeWave *bw : bladeWaves) {
        delete bw;
    }
    bladeWaves.clear();

    // 清理所有GIF刀浪
    for (DaolangWave *dw : daolangWaves) {
        delete dw;
    }
    daolangWaves.clear();

    // 清理玄武盾
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    // 清理背后火焰
    if (fireBgItem) {
        delete fireBgItem;
        fireBgItem = nullptr;
    }

    // 清理宠物
    if (pet) {
        delete pet;
        pet = nullptr;
    }

    // 清理所有敌人
    for (Enemy *e : enemies) {
        delete e;
    }
    enemies.clear();

    // 清理所有敌人炮弹
    for (EnemyProjectile *ep : enemyProjectiles) {
        delete ep;
    }
    enemyProjectiles.clear();

    // 清理所有巢穴
    for (Spawner *s : spawners) {
        delete s;
    }
    spawners.clear();

    // 清理 HUD
    if (hudHpBg) { delete hudHpBg; hudHpBg = nullptr; }
    if (hudHpFg) { delete hudHpFg; hudHpFg = nullptr; }
    if (hudMpBg) { delete hudMpBg; hudMpBg = nullptr; }
    if (hudMpFg) { delete hudMpFg; hudMpFg = nullptr; }
    if (hudText) { delete hudText; hudText = nullptr; }
    if (hudLevelText) { delete hudLevelText; hudLevelText = nullptr; }

    delete tileMap;
    delete player;
}

void Game::loadMap(const QString &mapFilePath, bool useStartPoint)
{
    qDebug() << "[loadMap] Loading map:" << mapFilePath << "useStartPoint:" << useStartPoint;

    // ---------- 1. 暂停游戏循环，避免重建期间 updateGame 访问野指针 ----------
    if (gameTimer) {
        gameTimer->stop();
        qDebug() << "[loadMap] Game timer stopped.";
    }

    // ---------- 2. 清理所有现有资源 ----------
    // 清理旧地图
    if (tileMap) {
        delete tileMap;
        tileMap = nullptr;
        qDebug() << "[loadMap] Old tileMap deleted.";
    }
    if (player) {
        if (player->scene()) scene->removeItem(player);
        delete player;
        player = nullptr;
        qDebug() << "[loadMap] Old player deleted.";
    }

    // 清理所有流星粒子
    for (Projectile *p : projectiles) {
        delete p;
    }
    projectiles.clear();

    // 清理所有刀浪
    for (BladeWave *bw : bladeWaves) {
        delete bw;
    }
    bladeWaves.clear();

    // 清理玄武盾
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    // 清理所有敌人
    for (Enemy *e : enemies) {
        delete e;
    }
    enemies.clear();

    // 清理所有敌人炮弹
    for (EnemyProjectile *ep : enemyProjectiles) {
        delete ep;
    }
    enemyProjectiles.clear();

    // 清理所有巢穴
    for (Spawner *s : spawners) {
        delete s;
    }
    spawners.clear();

    // 清理宠物（场景清空后旧宠物已失效，需要重建）
    if (pet) {
        delete pet;
        pet = nullptr;
        qDebug() << "[loadMap] Old pet deleted.";
    }

    // 清理背后火焰
    if (fireBgItem) {
        delete fireBgItem;
        fireBgItem = nullptr;
    }
    fireBgFrameIdx = 0;
    fireBgTick = 0;

    // 清理 HUD
    if (hudHpBg) { delete hudHpBg; hudHpBg = nullptr; }
    if (hudHpFg) { delete hudHpFg; hudHpFg = nullptr; }
    if (hudMpBg) { delete hudMpBg; hudMpBg = nullptr; }
    if (hudMpFg) { delete hudMpFg; hudMpFg = nullptr; }
    if (hudExpBg) { delete hudExpBg; hudExpBg = nullptr; }
    if (hudExpFg) { delete hudExpFg; hudExpFg = nullptr; }
    if (hudText) { delete hudText; hudText = nullptr; }
    if (hudLevelText) { delete hudLevelText; hudLevelText = nullptr; }

    // 清空可攻击对象列表
    hittableItems.clear();

    // 清除场景中所有已有项（瓦片、碰撞体等）
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        scene->removeItem(item);
        delete item;
    }
    qDebug() << "[loadMap] Scene cleared.";

    // loadMap 函数开头，清理旧数据的地方添加
    fireRects.clear();

    // ---------- 3. 创建新地图 ----------
    tileMap = new TileMap();
    if (!tileMap->loadFromFile(mapFilePath)) {
        qDebug() << "[loadMap] Failed to load map:" << mapFilePath;
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
        // 重新启动定时器
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
        }
        gameTimer->start(16);
        qDebug() << "[loadMap] Fallback: gray background + blue player, timer started.";
        return;
    }
    qDebug() << "[loadMap] TileMap loaded successfully.";

    // ================= 手动绘制所有图层（包括 floor 和 wall）=================
    QFile file(mapFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isNull()) {
            QJsonObject root = doc.object();
            int mapWidth = root["width"].toInt();
            int mapHeight = root["height"].toInt();
            int tileWidth = root["tilewidth"].toInt();
            int tileHeight = root["tileheight"].toInt();

            // 图层名 -> 图片路径映射（为所有图层提供默认图片）
            QMap<QString, QString> layerImageMap;
            // 通用装饰层
            layerImageMap["door"]   = ":/images/door.png";
            layerImageMap["chest"]  = ":/images/chest.png";
            layerImageMap["boss"]   = ":/images/boss.png";
            layerImageMap["boss_image"] = ":/images/boss.png";
            layerImageMap["portal"] = ":/images/portal.png";
            layerImageMap["portal_image"] = ":/images/portal.png";
            layerImageMap["minion"] = ":/images/minion.png";
            layerImageMap["minion_image"] = ":/images/minion.png";
            layerImageMap["water"]  = ":/images/water.png";
            layerImageMap["grass"]  = ":/images/grass.png";
            layerImageMap["rock"]   = ":/images/rock.png";
            layerImageMap["fireland"] = ":/images/fire.png";
            layerImageMap["elite"]  = ":/images/elite.png";
            layerImageMap["stair"]  = ":/images/stair.png";
            // floor 和 wall 不使用固定图片，而是随机纹理，在循环中单独处理

            QJsonArray layers = root["layers"].toArray();
            for (const QJsonValue &layerVal : layers) {
                QJsonObject layerObj = layerVal.toObject();
                QString layerName = layerObj["name"].toString();
                if (layerObj["type"].toString() != "tilelayer") continue;

                QJsonArray dataArr = layerObj["data"].toArray();
                if (dataArr.size() != mapWidth * mapHeight) continue;

                // ========== 处理 minion / minion_image 图层（生成 Enemy）==========
                if (layerName == "minion" || layerName == "minion_image") {
                    int enemyCount = 0;
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;
                            Enemy *enemy = new Enemy(tileMap, scene,
                                                     QPointF(x * tileWidth, y * tileHeight),
                                                     this);
                            enemies.append(enemy);
                            hittableItems.append(enemy);
                            enemyCount++;
                        }
                    }
                    qDebug() << "[ManualDraw] Minion layer" << layerName << "created" << enemyCount << "enemies";
                    continue;
                }

                // ========== 处理 water 图层（阻挡玩家，不阻挡子弹）==========
                if (layerName == "water") {
                    QVector<Tile*> waterTiles;   // 临时数组
                    int waterCount = 0;
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;
                            Tile *waterTile = new Tile(layerImageMap["water"], x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            scene->addItem(waterTile);
                            waterTiles.append(waterTile);   // 先收集，不调用 addWaterTile
                            waterCount++;
                        }
                    }
                    // 批量添加到 tileMap，一次性重建网格
                    if (!waterTiles.isEmpty()) {
                        tileMap->addWaterTiles(waterTiles);
                    }
                    qDebug() << "[ManualDraw] Water layer created" << waterCount << "water tiles";
                    continue;
                }

                // ========== 处理 fireland 图层（持续掉血）==========
                if (layerName == "fireland") {
                    int fireCount = 0;
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;
                            // 记录火焰区域的矩形（用于每帧检测）
                            fireRects.append(QRectF(x * tileWidth, y * tileHeight, tileWidth, tileHeight));
                            // 同时创建视觉效果（显示火焰图片）
                            Tile *fireTile = new Tile(layerImageMap["fireland"], x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            scene->addItem(fireTile);
                            fireCount++;
                        }
                    }
                    qDebug() << "[ManualDraw] Fireland layer created" << fireCount << "tiles";
                    continue;  // 跳过普通瓦片创建
                }

                // ========== 普通瓦片图层（包括 floor, wall 和所有装饰）==========
                int tileCount = 0;
                for (int y = 0; y < mapHeight; ++y) {
                    for (int x = 0; x < mapWidth; ++x) {
                        int rawGid = dataArr[y * mapWidth + x].toInt();
                        int cleanGid = rawGid & 0x1FFFFFFF;
                        if (cleanGid == 0) continue;

                        QString finalPath;
                        QSize fixedSize(tileWidth, tileHeight);

                        // 地板随机纹理
                        if (layerName == "floor") {
                            finalPath = (QRandomGenerator::global()->bounded(2) == 0)
                                        ? ":/images/floor_dark.png" : ":/images/floor_light.png";
                        }
                        // 墙壁随机纹理
                        else if (layerName == "wall") {
                            int r = QRandomGenerator::global()->bounded(3);
                            finalPath = QString(":/images/wall_%1.png").arg(r + 1);
                        }
                        // 其他图层：从映射表获取或使用默认图片
                        else {
                            finalPath = layerImageMap.value(layerName, "");
                            if (finalPath.isEmpty()) {
                                finalPath = ":/images/" + layerName + ".png";
                                qDebug() << "[ManualDraw] No mapping for layer" << layerName << ", using" << finalPath;
                            }
                        }

                        Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                        scene->addItem(tile);
                        tileCount++;

                        // 墙壁需要加入碰撞系统
                        if (layerName == "wall") {
                            tileMap->addWallTile(tile);
                        }

                        // Boss / elite 图块加入可攻击列表
                        if (layerName == "boss" || layerName == "boss_image" || layerName == "elite") {
                            hittableItems.append(tile);
                        }
                    }
                }
                qDebug() << "[ManualDraw] Layer" << layerName << "drew" << tileCount << "tiles";
            }
        } else {
            qDebug() << "[ManualDraw] Failed to parse JSON for manual drawing:" << mapFilePath;
        }
        file.close();
    } else {
        qDebug() << "[ManualDraw] Cannot open map file for manual drawing:" << mapFilePath;
    }

    // ---------- 4. 创建玩家 ----------
    player = new Player(tileMap);
    scene->addItem(player);
    connect(player, &Player::died, this, &Game::onPlayerDied);   // 连接死亡信号
    qDebug() << "[loadMap] Player created.";

    // ---------- 5. 创建背后火焰（如果已解锁）----------
    if (!fireBgItem && !g_fireBgFrames.isEmpty()) {
        fireBgItem = new QGraphicsPixmapItem();
        scene->addItem(fireBgItem);
        fireBgItem->setTransformationMode(Qt::SmoothTransformation);
        fireBgItem->setZValue(1);
        fireBgItem->setPixmap(g_fireBgFrames[0]);
        qDebug() << "[loadMap] Fire background created.";
    }

    // ---------- 6. 创建宠物（全新创建）----------
    QPointF playerStart = tileMap->getPlayerStart();
    pet = new Pet(scene, tileMap);
    pet->setPos(playerStart + QPointF(40, 0));
    if (player) pet->stackBefore(player);
    qDebug() << "[loadMap] Pet created at position:" << pet->pos();

    // ---------- 7. 创建巢穴（基于玩家出生点）----------
    QPointF spawnBase = playerStart;
    if (spawnBase.isNull()) spawnBase = QPointF(100, 100);
    spawners.append(new Spawner(tileMap, scene, spawnBase + QPointF(300, 100), this));
    spawners.append(new Spawner(tileMap, scene, spawnBase + QPointF(-100, 300), this));
    spawners.append(new Spawner(tileMap, scene, spawnBase + QPointF(-200, -100), this));
    qDebug() << "[loadMap] 3 spawners created.";

    // ---------- 8. 连接玩家升级信号 ----------
    connect(player, &Player::levelUp, this, &Game::onPlayerLevelUp);

    // ---------- 9. 设置玩家初始位置 ----------
    if (useStartPoint) {
        QPointF startPos = tileMap->getPlayerStart();
        if (startPos.isNull()) {
            startPos = QPointF(100, 100);
        }
        player->setPos(startPos);
        qDebug() << "[loadMap] Player placed at start point:" << startPos;
    } else {
        // 临时置零，稍后由跨地图传送逻辑覆盖位置
        player->setPos(0, 0);
        qDebug() << "[loadMap] Player position temporarily set to (0,0), will be overwritten by portal.";
    }

    currentMapPath = mapFilePath;
    qDebug() << "[loadMap] Current map path set to:" << currentMapPath;

    // ---------- 10. 设置场景矩形 ----------
    // 动态计算场景矩形（像素为单位）
    int mapPixelWidth = tileMap->getMapWidth() * tileMap->getTileWidth();
    int mapPixelHeight = tileMap->getMapHeight() * tileMap->getTileHeight();
    scene->setSceneRect(0, 0, mapPixelWidth, mapPixelHeight);
    setSceneRect(scene->sceneRect());
    qDebug() << "[loadMap] Scene rect set to:" << mapPixelWidth << "x" << mapPixelHeight;

    if (useStartPoint) {
        centerOn(player);
    }

    // 重置缩放
    zoomLevel = 1.0;
    applyZoom();

    // 创建 HUD
    createHud();
    qDebug() << "[loadMap] HUD created.";

    // ---------- 11. 重新启动游戏循环 ----------
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
        qDebug() << "[loadMap] Game timer created.";
    }
    gameTimer->start(16);
    qDebug() << "[loadMap] Game timer started (16ms interval).";

    qDebug() << "[loadMap] Map loading completed.";
}

void Game::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_W: upPressed = true; break;
    case Qt::Key_S: downPressed = true; break;
    case Qt::Key_A: leftPressed = true; break;
    case Qt::Key_D: rightPressed = true; break;
    case Qt::Key_I: skillMeteorBurst(); break;   // ← I键：天火燎原技能（一技能，静止时才能使用）
    case Qt::Key_H: skillTriangleShot(); break;  // ← H键：单方向破空梭（朝移动方向）
    case Qt::Key_N: skillBlueBurst(); break;     // ← N键：普攻2（蓝色八方向月牙，可边移动边发射）
    case Qt::Key_J: skillNormalAttack(); break;  // ← J键：普攻（九重炎杀）
    case Qt::Key_K: skillFlashBlade(); break;    // ← K键：瞬影浪斩技能（二技能）
    case Qt::Key_L: skillShieldActivate(); break;// ← L键：激活玄武盾（三技能）
    case Qt::Key_Plus:
    case Qt::Key_Equal:  // 兼容主键盘 =/+ 键
        zoomLevel *= ZOOM_STEP;
        if (zoomLevel > MAX_ZOOM) zoomLevel = MAX_ZOOM;
        applyZoom();
        break;
    case Qt::Key_Minus:
        zoomLevel /= ZOOM_STEP;
        if (zoomLevel < MIN_ZOOM) zoomLevel = MIN_ZOOM;
        applyZoom();
        break;
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
    case Qt::Key_L: skillShieldDeactivate(); break; // ← L键释放：关闭玄武盾
    default: QGraphicsView::keyReleaseEvent(event);
    }
}

void Game::updateGame()
{
    // 变身动画期间暂停游戏
    if (gamePaused) return;

    // ========== 闪现动画（优先处理）==========
    if (flashState.active && player) {
        player->setPos(player->pos() + flashState.step);
        flashState.framesLeft--;

        // 每帧生成一个红色残影
        QGraphicsEllipseItem *dot = new QGraphicsEllipseItem(-4, -4, 8, 8);
        dot->setPos(player->sceneBoundingRect().center());
        dot->setBrush(QBrush(QColor(255, 50, 50, 200)));
        dot->setPen(Qt::NoPen);
        scene->addItem(dot);
        QTimer::singleShot(200, [dot]() { delete dot; });

        if (flashState.framesLeft <= 0) {
            // 闪现结束，校正到最终位置
            flashState.active = false;
            player->setPos(flashState.finalPos);

            // 射出刀浪
            qreal bladeSpeed = 12.0;
            int damage = 40;
            QPointF bladeStart = player->sceneBoundingRect().center()
                                 + QPointF(flashState.bladeDir.x() * 20.0, flashState.bladeDir.y() * 20.0);
            QPointF bladeVelocity(flashState.bladeDir.x() * bladeSpeed, flashState.bladeDir.y() * bladeSpeed);

            if (player->getLevel() >= 2) {
                // 2级+：使用GIF刀浪
                DaolangWave *dw = new DaolangWave(bladeStart, bladeVelocity, damage, tileMap, scene);
                daolangWaves.append(dw);
            } else {
                // 1级：矩形刀浪
                BladeWave *bw = new BladeWave(bladeStart, bladeVelocity, damage, tileMap, scene);
                bladeWaves.append(bw);
            }

            // 闪现后检查宠物距离，超出则重置
            if (pet) {
                qreal dist = QLineF(pet->pos(), player->pos()).length();
                if (dist > 160.0) pet->resetToOwner(player->pos());
            }
        }
        centerOn(player);
    }

    // 正常移动
    else if (player) {
        QPointF oldPos = player->pos();                    // 记录移动前位置
        player->move(upPressed, downPressed, leftPressed, rightPressed);
        // ========== 新增：水碰撞回退 ==========
        if (tileMap->collidesWithWater(player->hitboxRect())) {
            player->setPos(oldPos);                        // 回退到移动前
        }
        player->updateCastAnimation();
        centerOn(player);
    }

    // 每 20 帧（约 0.33 秒）恢复 1 HP 和 1 MP，速度为原来的 3 倍
    regenCounter++;
    if (regenCounter >= 20) {
        regenCounter = 0;
        if (player) {
            player->recoverHpMp(1, 1);
        }
    }

    updateProjectiles();        // ← 更新所有流星粒子
    updateSimpleProjectiles();  // ← 更新所有简易子弹
    updateBlueProjectiles();    // ← 更新所有冰魄八荒子弹
    updateTriangleProjectiles();// ← 更新所有破空梭
    updateBladeWaves();         // ← 更新所有刀浪
    updateDaolangWaves();       // ← 更新所有GIF刀浪
    updateShieldPosition();    // ← 更新玄武盾跟随玩家
    applyTerrainEffects();

    // 更新宠物
    if (pet && player) {
        pet->update(player->pos());
    }

    // 更新背后火焰动画和位置
    if (fireBgItem && player && !g_fireBgFrames.isEmpty()) {
        QRectF playerRect = player->sceneBoundingRect();
        fireBgItem->setPos(playerRect.center() + QPointF(-40, -40));
        fireBgTick++;
        if (fireBgTick >= 3) {
            fireBgTick = 0;
            fireBgFrameIdx = (fireBgFrameIdx + 1) % g_fireBgFrames.size();
            fireBgItem->setPixmap(g_fireBgFrames[fireBgFrameIdx]);
        }
    }

    updateEnemies();           // ← 更新所有敌人
    updateEnemyProjectiles();  // ← 更新所有敌人炮弹
    updateSpawners();          // ← 更新所有巢穴
    updateHud();               // ← 更新 HUD 位置和数值
    checkPortal();
}

void Game::checkPortal()
{
    if (!canTeleport || isTeleporting) return;
    if (!player || !tileMap) return; // 安全检查

    QRectF playerRect = player->hitboxRect();
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

void Game::skillMeteorBurst()
{
    if (!player) return;
    if (!player->consumeMp(10)) return; // 消耗 10 MP，不足则无法释放

    // I技能：不移动时才能使用
    if (upPressed || downPressed || leftPressed || rightPressed) return;

    // 变身形态下播放飞火施法动画（间隔1帧，更快）
    if (player->getEnhanced()) {
        player->playCastAnimation(":/images/player_enhanced_fly_fire.gif", 1);
    }

    // 以玩家中心为发射原点
    QPointF center = player->sceneBoundingRect().center();

    qreal speed = 8.0;       // 火炮飞行速度（像素/帧）
    int damage = 25;         // 伤害值
    int level = player->getLevel();

    // 8 个方向：上、右上、右、右下、下、左下、左、左上
    QVector<QPointF> directions = {
        QPointF(0, -speed),                           // 上
        QPointF(speed * 0.707, -speed * 0.707),       // 右上
        QPointF(speed, 0),                            // 右
        QPointF(speed * 0.707, speed * 0.707),        // 右下
        QPointF(0, speed),                            // 下
        QPointF(-speed * 0.707, speed * 0.707),       // 左下
        QPointF(-speed, 0),                           // 左
        QPointF(-speed * 0.707, -speed * 0.707)       // 左上
    };

    for (const QPointF &dir : directions) {
        if (level >= 3) {
            // 3级+：发射 GIF 飞火弹
            Projectile *p = new Projectile(center, dir, damage, tileMap, scene);
            projectiles.append(p);
        } else {
            // 1-2级：发射简易红色椭圆子弹
            SimpleProjectile *sp = new SimpleProjectile(center, dir, damage, tileMap, scene);
            simpleProjectiles.append(sp);
        }
    }
}

void Game::skillBlueBurst()
{
    if (!player) return;

    // H键：普攻2，蓝色八方向月牙子弹，可边移动边发射
    QPointF center = player->sceneBoundingRect().center();
    qreal speed = 8.0;
    int damage = 15;           // 伤害略低于I技能

    QVector<QPointF> directions = {
        QPointF(0, -speed),
        QPointF(speed * 0.707, -speed * 0.707),
        QPointF(speed, 0),
        QPointF(speed * 0.707, speed * 0.707),
        QPointF(0, speed),
        QPointF(-speed * 0.707, speed * 0.707),
        QPointF(-speed, 0),
        QPointF(-speed * 0.707, -speed * 0.707)
    };

    for (const QPointF &dir : directions) {
        BlueProjectile *bp = new BlueProjectile(center, dir, damage, tileMap, scene);
        blueProjectiles.append(bp);
    }
}

void Game::updateProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的粒子
    for (int i = projectiles.size() - 1; i >= 0; --i) {
        Projectile *p = projectiles[i];
        bool alive = p->update();

        // 检测是否击中可攻击对象（Boss、小怪等）
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (p->collidesWithItem(hittable)) {
                    // 对 Enemy 造成实际伤害
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(p->getDamage());
                    }
                    qDebug() << "Hit! Damage:" << p->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            if (explosionsEnabled) {
                createExplosion(p->sceneBoundingRect().center());
            }
            delete p;
            projectiles.removeAt(i);
        }
    }
}

void Game::updateSimpleProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的简易子弹
    for (int i = simpleProjectiles.size() - 1; i >= 0; --i) {
        SimpleProjectile *sp = simpleProjectiles[i];
        bool alive = sp->update();

        // 检测是否击中可攻击对象
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (sp->collidesWithItem(hittable)) {
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(sp->getDamage());
                    }
                    qDebug() << "Simple projectile hit! Damage:" << sp->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete sp;
            simpleProjectiles.removeAt(i);
        }
    }
}

void Game::updateBlueProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的冰魄八荒子弹
    for (int i = blueProjectiles.size() - 1; i >= 0; --i) {
        BlueProjectile *bp = blueProjectiles[i];
        bool alive = bp->update();

        // 检测是否击中可攻击对象
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (bp->collidesWithItem(hittable)) {
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(bp->getDamage());
                    }
                    qDebug() << "Blue crescent hit! Damage:" << bp->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete bp;
            blueProjectiles.removeAt(i);
        }
    }
}

void Game::updateTriangleProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的破空梭
    for (int i = triangleProjectiles.size() - 1; i >= 0; --i) {
        TriangleProjectile *tp = triangleProjectiles[i];
        bool alive = tp->update();

        // 检测是否击中可攻击对象
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (tp->collidesWithItem(hittable)) {
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(tp->getDamage());
                    }
                    qDebug() << "Triangle hit! Damage:" << tp->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete tp;
            triangleProjectiles.removeAt(i);
        }
    }
}

void Game::skillTriangleShot()
{
    if (!player) return;

    QPointF dir = getCurrentDirectionVector();
    qreal speed = 10.0;
    int damage = 45; // J技能伤害15的3倍

    QPointF velocity(dir.x() * speed, dir.y() * speed);
    QPointF start = player->sceneBoundingRect().center()
                    + QPointF(dir.x() * 20.0, dir.y() * 20.0);

    TriangleProjectile *tp = new TriangleProjectile(start, velocity, damage, tileMap, scene);
    triangleProjectiles.append(tp);
}

void Game::createExplosion(QPointF centerPos)
{
    if (!scene || g_bombFrames.isEmpty()) return;

    QGraphicsPixmapItem *item = new QGraphicsPixmapItem();
    item->setTransformationMode(Qt::SmoothTransformation);
    scene->addItem(item);
    item->setPos(centerPos.x() - 48, centerPos.y() - 48);
    item->setPixmap(g_bombFrames[0]);

    QTimer *timer = new QTimer(this);
    int *frameIdx = new int(0);

    connect(timer, &QTimer::timeout, [timer, item, frameIdx]() {
        (*frameIdx)++;
        if (*frameIdx >= g_bombFrames.size()) {
            timer->stop();
            timer->deleteLater();
            if (item->scene()) item->scene()->removeItem(item);
            delete item;
            delete frameIdx;
            return;
        }
        item->setPixmap(g_bombFrames[*frameIdx]);
    });

    timer->start(8); // 8ms 一帧，约 125fps
}

QPointF Game::getCurrentDirectionVector()
{
    qreal dx = 0.0;
    qreal dy = 0.0;
    if (rightPressed) dx += 1.0;
    if (leftPressed)  dx -= 1.0;
    if (downPressed)  dy += 1.0;
    if (upPressed)    dy -= 1.0;

    // 如果没有方向键被按下，默认向右
    if (dx == 0.0 && dy == 0.0) {
        dx = 1.0;
    }

    // 归一化（保证斜向速度大小与正方向一致）
    qreal len = qSqrt(dx * dx + dy * dy);
    if (len > 0.0) {
        dx /= len;
        dy /= len;
    }
    return QPointF(dx, dy);
}

void Game::skillFlashBlade()
{
    if (!player || !tileMap) return;
    if (flashState.active) return; // 闪现中不能再次闪现
    if (!player->consumeMp(15)) return; // 消耗 15 MP，不足则无法释放

    // ========== 1. 检查是否有方向键被按下（静止时不触发）==========
    if (!upPressed && !downPressed && !leftPressed && !rightPressed) {
        return;
    }

    QPointF dir = getCurrentDirectionVector();

    // ========== 2. 计算闪现目标位置（步进法，不能穿墙）==========
    qreal flashDistance = 100.0; // 最大闪现距离
    qreal step = 4.0;            // 每步检测 4 像素
    QPointF oldPos = player->pos(); // 闪现前左上角
    QPointF currentPos = oldPos;
    QPointF finalPos = oldPos;

    for (qreal dist = step; dist <= flashDistance; dist += step) {
        QPointF testPos = oldPos + QPointF(dir.x() * dist, dir.y() * dist);
        player->setPos(testPos);
        if (tileMap->collidesWithWall(player->hitboxRect())) {
            finalPos = currentPos;
            break;
        }
        currentPos = testPos;
        finalPos = testPos;
    }

    // 把玩家位置恢复为 oldPos，由 updateGame 中的闪现动画逐步移动
    player->setPos(oldPos);

    // ========== 3. 变身后伴随普攻动画 ==========
    if (player->getEnhanced()) {
        QMovie *pugongMovie = new QMovie(":/images/player_enhanced_K.gif");
        QGraphicsPixmapItem *pugongItem = new QGraphicsPixmapItem();
        scene->addItem(pugongItem);
        pugongItem->setTransformationMode(Qt::SmoothTransformation);
        pugongItem->setZValue(50);
        pugongItem->setPos(player->pos() + QPointF(32, 32)); // 玩家中心

        connect(pugongMovie, &QMovie::frameChanged, [this, pugongItem, pugongMovie](int frame) {
            QPixmap pixmap = pugongMovie->currentPixmap();
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                pugongItem->setPixmap(pixmap);
                pugongItem->setOffset(-pixmap.width() / 2.0, -pixmap.height() / 2.0);
            }
            if (frame >= pugongMovie->frameCount() - 1) {
                pugongMovie->stop();
                if (pugongItem->scene()) scene->removeItem(pugongItem);
                delete pugongItem;
                pugongMovie->deleteLater();
            }
        });
        pugongMovie->start();
    }

    // ========== 4. 设置跨帧闪现状态 ==========
    const int FLASH_FRAMES = 5;
    flashState.active = true;
    flashState.step = (finalPos - oldPos) / FLASH_FRAMES;
    flashState.framesLeft = FLASH_FRAMES;
    flashState.finalPos = finalPos;
    flashState.bladeDir = dir;
}

void Game::updateBladeWaves()
{
    // 倒序遍历，方便安全删除已死亡的刀浪
    for (int i = bladeWaves.size() - 1; i >= 0; --i) {
        BladeWave *bw = bladeWaves[i];
        bool alive = bw->update();

        // 检测是否击中可攻击对象（Boss、小怪等）
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (bw->collidesWithItem(hittable)) {
                    // 对 Enemy 造成实际伤害
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(bw->getDamage());
                    }
                    qDebug() << "BladeWave Hit! Damage:" << bw->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete bw;
            bladeWaves.removeAt(i);
        }
    }
}

void Game::updateDaolangWaves()
{
    // 倒序遍历，方便安全删除已死亡的GIF刀浪
    for (int i = daolangWaves.size() - 1; i >= 0; --i) {
        DaolangWave *dw = daolangWaves[i];
        bool alive = dw->update();

        // 检测是否击中可攻击对象
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (dw->collidesWithItem(hittable)) {
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(dw->getDamage());
                    }
                    qDebug() << "DaolangWave Hit! Damage:" << dw->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete dw;
            daolangWaves.removeAt(i);
        }
    }
}

void Game::skillNormalAttack()
{
    if (!player || !scene) return;

    // 变身形态下播放普攻施法动画
    if (player->getEnhanced()) {
        player->playCastAnimation(":/images/player_enhanced_pugong.gif");
    }

    // ========== 1. 九宫格攻击范围 ==========
    QPointF playerCenter = player->sceneBoundingRect().center();
    QRectF attackRect(playerCenter.x() - 48.0, playerCenter.y() - 48.0, 96.0, 96.0);

    // ========== 2. 普攻 GIF 特效（持续 2 秒后消失）==========
    QMovie *hitMovie = new QMovie(":/images/fire_hit.gif");
    QGraphicsPixmapItem *hitItem = new QGraphicsPixmapItem();
    hitItem->setTransformationMode(Qt::SmoothTransformation);
    scene->addItem(hitItem);

    connect(hitMovie, &QMovie::frameChanged, [hitItem, hitMovie](int) {
        QPixmap frame = hitMovie->currentPixmap();
        if (!frame.isNull()) {
            frame = frame.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            hitItem->setPixmap(frame);
        }
    });

    // 动画停止后延迟清理，避免在信号处理中直接销毁 QMovie
    connect(hitMovie, &QMovie::stateChanged, [hitItem, hitMovie](QMovie::MovieState state) {
        if (state == QMovie::NotRunning) {
            QTimer::singleShot(0, [hitItem, hitMovie]() {
                if (hitItem->scene()) hitItem->scene()->removeItem(hitItem);
                delete hitItem;
                delete hitMovie;
            });
        }
    });

    // 2 秒后延迟停止（让火焰持续显示 2 秒）
    QTimer::singleShot(2000, [hitMovie]() {
        if (hitMovie->state() != QMovie::NotRunning) {
            QTimer::singleShot(0, hitMovie, &QMovie::stop);
        }
    });

    hitMovie->start();
    hitItem->setPos(playerCenter.x() - 48, playerCenter.y() - 48);

    // ========== 3. 伤害检测（九宫格范围内的 hittable 对象）==========
    for (QGraphicsItem *hittable : hittableItems) {
        QRectF hittableRect = hittable->boundingRect().translated(hittable->pos());
        if (attackRect.intersects(hittableRect)) {
            Enemy *enemy = dynamic_cast<Enemy*>(hittable);
            if (enemy) {
                enemy->takeDamage(15);
            }
            qDebug() << "Normal Attack Hit! Damage: 15"
                     << "to hittable object at" << hittable->pos();
        }
    }
}

void Game::skillShieldActivate()
{
    if (!player || !scene || shieldItem) return;
    if (!player->consumeMp(5)) return; // 开启玄武盾消耗 5 MP

    // 创建玄武盾：比玩家稍大的圆形（半径 28px）
    shieldItem = new QGraphicsEllipseItem(-28, -28, 56, 56);
    shieldItem->setPos(player->sceneBoundingRect().center());
    // 玄武盾颜色：半透明青蓝色 + 发光边框
    shieldItem->setBrush(QBrush(QColor(100, 180, 255, 80)));
    shieldItem->setPen(QPen(QColor(150, 220, 255, 150), 3));
    scene->addItem(shieldItem);
    shieldActive = true;

    qDebug() << "Shield activated!";
}

void Game::skillShieldDeactivate()
{
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    qDebug() << "Shield deactivated.";
}

void Game::updateShieldPosition()
{
    if (shieldActive && shieldItem && player) {
        shieldItem->setPos(player->sceneBoundingRect().center());
    }
}

void Game::applyZoom()
{
    resetTransform();
    scale(zoomLevel, zoomLevel);
    if (player) {
        centerOn(player);
    }
}

void Game::createHud()
{
    if (!scene) return;

    // 血条背景（灰色）
    hudHpBg = new QGraphicsRectItem(0, 0, 120, 14);
    hudHpBg->setBrush(QBrush(QColor(60, 60, 60, 200)));
    hudHpBg->setPen(QPen(Qt::black, 1));
    scene->addItem(hudHpBg);

    // 血条前景（红色）
    hudHpFg = new QGraphicsRectItem(0, 0, 120, 14);
    hudHpFg->setBrush(QBrush(QColor(220, 60, 60, 230)));
    hudHpFg->setPen(Qt::NoPen);
    scene->addItem(hudHpFg);

    // 蓝条背景（灰色）
    hudMpBg = new QGraphicsRectItem(0, 0, 120, 14);
    hudMpBg->setBrush(QBrush(QColor(60, 60, 60, 200)));
    hudMpBg->setPen(QPen(Qt::black, 1));
    scene->addItem(hudMpBg);

    // 蓝条前景（蓝色）
    hudMpFg = new QGraphicsRectItem(0, 0, 120, 14);
    hudMpFg->setBrush(QBrush(QColor(60, 120, 220, 230)));
    hudMpFg->setPen(Qt::NoPen);
    scene->addItem(hudMpFg);

    // 经验条背景（灰色）
    hudExpBg = new QGraphicsRectItem(0, 0, 120, 10);
    hudExpBg->setBrush(QBrush(QColor(60, 60, 60, 200)));
    hudExpBg->setPen(QPen(Qt::black, 1));
    scene->addItem(hudExpBg);

    // 经验条前景（金黄色）
    hudExpFg = new QGraphicsRectItem(0, 0, 120, 10);
    hudExpFg->setBrush(QBrush(QColor(218, 165, 32, 230)));
    hudExpFg->setPen(Qt::NoPen);
    scene->addItem(hudExpFg);

    // 文字
    hudText = new QGraphicsSimpleTextItem();
    hudText->setBrush(QBrush(Qt::white));
    QFont font = hudText->font();
    font.setPointSize(10);
    font.setBold(true);
    hudText->setFont(font);
    scene->addItem(hudText);

    // 等级文字
    hudLevelText = new QGraphicsSimpleTextItem();
    hudLevelText->setBrush(QBrush(Qt::yellow));
    QFont lvlFont = hudLevelText->font();
    lvlFont.setPointSize(11);
    lvlFont.setBold(true);
    hudLevelText->setFont(lvlFont);
    scene->addItem(hudLevelText);
}

void Game::updateHud()
{
    if (!player || !hudHpBg) return;

    // 将视图左上角坐标转换为场景坐标，使 HUD 固定在屏幕左上角
    QPointF hudPos = mapToScene(10, 10);

    // 更新血条宽度
    qreal hpRatio = static_cast<qreal>(player->getHp()) / player->getMaxHp();
    if (hpRatio < 0) hpRatio = 0;
    hudHpFg->setRect(hudPos.x(), hudPos.y(), 120 * hpRatio, 14);
    hudHpBg->setRect(hudPos.x(), hudPos.y(), 120, 14);

    // 更新蓝条宽度
    qreal mpRatio = static_cast<qreal>(player->getMp()) / player->getMaxMp();
    if (mpRatio < 0) mpRatio = 0;
    hudMpFg->setRect(hudPos.x(), hudPos.y() + 18, 120 * mpRatio, 14);
    hudMpBg->setRect(hudPos.x(), hudPos.y() + 18, 120, 14);

    // 更新经验条宽度
    qreal expRatio = static_cast<qreal>(player->getExp()) / player->getMaxExp();
    if (expRatio < 0) expRatio = 0;
    hudExpFg->setRect(hudPos.x(), hudPos.y() + 34, 120 * expRatio, 10);
    hudExpBg->setRect(hudPos.x(), hudPos.y() + 34, 120, 10);

    // 更新文字
    QString text = QString("HP:%1/%2  MP:%3/%4")
                       .arg(player->getHp()).arg(player->getMaxHp())
                       .arg(player->getMp()).arg(player->getMaxMp());
    hudText->setText(text);
    hudText->setPos(hudPos.x() + 2, hudPos.y() + 46);

    // 更新等级文字
    QString lvlText = QString("LV.%1  EXP:%2/%3")
                          .arg(player->getLevel())
                          .arg(player->getExp())
                          .arg(player->getMaxExp());
    hudLevelText->setText(lvlText);
    hudLevelText->setPos(hudPos.x() + 2, hudPos.y() + 62);

    // 确保 HUD 在最上层
    hudHpBg->setZValue(1000);
    hudHpFg->setZValue(1001);
    hudMpBg->setZValue(1000);
    hudMpFg->setZValue(1001);
    hudExpBg->setZValue(1000);
    hudExpFg->setZValue(1001);
    hudText->setZValue(1002);
    hudLevelText->setZValue(1002);
}

void Game::addEnemyProjectile(EnemyProjectile *ep)
{
    if (ep) {
        enemyProjectiles.append(ep);
    }
}

void Game::updateEnemies()
{
    // 根据玩家等级调整所有敌人的攻击间隔（等级越高，怪物射得越快）
    int playerLevel = player ? player->getLevel() : 1;
    int newInterval = qMax(30, 120 - (playerLevel - 1) * 15);

    // 倒序遍历，方便安全删除已死亡的敌人
    for (int i = enemies.size() - 1; i >= 0; --i) {
        Enemy *e = enemies[i];
        e->setAttackInterval(newInterval);
        e->update();
        if (e->isDead()) {
            // 从可攻击列表中移除
            hittableItems.removeAll(e);
            // 给玩家加经验
            if (player) {
                player->addExp(20);
            }
            delete e;
            enemies.removeAt(i);
        }
    }
}

void Game::updateSpawners()
{
    for (Spawner *s : spawners) {
        s->update();
    }
}

void Game::addEnemy(Enemy *e)
{
    if (e) {
        enemies.append(e);
        hittableItems.append(e);
    }
}

void Game::onPlayerLevelUp(int newLevel)
{
    qDebug() << "Player leveled up to" << newLevel;
    if (newLevel == 3) {
        // 3级：播放变身动画，结束后自动 setEnhanced(true)
        playTransformAnimation();
    }
    if (newLevel == 5) {
        // 5级：启用爆炸效果
        explosionsEnabled = true;
        qDebug() << "Level 5: explosions enabled!";
    }
}

void Game::applyLevel10Enhancement()
{
    // 原逻辑已合并到 onPlayerLevelUp，保留空实现兼容旧调用
}

void Game::playTransformAnimation()
{
    if (!scene || !player) return;

    gamePaused = true;
    transformMovie = new QMovie(":/images/player_tranform.gif");
    transformItem = new QGraphicsPixmapItem();
    transformItem->setTransformationMode(Qt::SmoothTransformation);
    transformItem->setZValue(9999);
    scene->addItem(transformItem);

    // 居中显示（基于视口中心对应的场景坐标）
    QPointF viewCenter = mapToScene(viewport()->rect().center());
    transformItem->setPos(viewCenter.x() - 400, viewCenter.y() - 388);

    connect(transformMovie, &QMovie::frameChanged, [this](int frameNumber) {
        QPixmap frame = transformMovie->currentPixmap();
        if (!frame.isNull()) {
            transformItem->setPixmap(frame);
        }
        if (transformMovie->frameCount() > 0 && frameNumber >= transformMovie->frameCount() - 1) {
            QTimer::singleShot(0, transformMovie, &QMovie::stop);
        }
    });

    connect(transformMovie, &QMovie::stateChanged, [this](QMovie::MovieState state) {
        if (state == QMovie::NotRunning) {
            QTimer::singleShot(0, [this]() {
                if (transformItem) {
                    if (transformItem->scene()) transformItem->scene()->removeItem(transformItem);
                    delete transformItem; transformItem = nullptr;
                }
                if (transformMovie) {
                    delete transformMovie; transformMovie = nullptr;
                }
                gamePaused = false;
                if (player) player->setEnhanced(true);
                qDebug() << "Transformation complete! Enhanced mode ON.";
            });
        }
    });

    transformMovie->start();
}

void Game::updateEnemyProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的炮弹
    for (int i = enemyProjectiles.size() - 1; i >= 0; --i) {
        EnemyProjectile *ep = enemyProjectiles[i];
        bool alive = ep->update(tileMap);

        // 检测是否击中玩家
        if (alive && player && ep->collidesWithItem(player)) {
            // 如果玄武盾激活，阻挡伤害
            if (shieldActive) {
                qDebug() << "Enemy projectile blocked by shield!";
            } else {
                player->takeDamage(ep->getDamage());
                qDebug() << "Player hit by enemy! Damage:" << ep->getDamage();
            }
            alive = false;
        }

        if (!alive) {
            delete ep;
            enemyProjectiles.removeAt(i);
        }
    }
}

void Game::performTeleport(const Portal &portal)
{
    // 二次确认玩家仍与传送门重叠（防止延迟期间玩家离开）
    if (!player || !tileMap) {
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    QRectF playerRect = player->hitboxRect();
    bool stillIntersects = false;
    for (const Portal &p : tileMap->getPortals()) {
        if (playerRect.intersects(p.rect)) {
            stillIntersects = true;
            break;
        }
    }
    if (!stillIntersects) {
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    // ----- 安全传送辅助函数（自动对齐碰撞框并防卡墙）-----
    auto safeTeleportTo = [&](const QPointF &targetCenter) {
        // 玩家显示 64x64，碰撞框为右下角 32x32，碰撞框中心相对于玩家左上角偏移 (48, 48)
        const int COLLISION_CENTER_OFFSET = 48;
        QPointF basePos = targetCenter - QPointF(COLLISION_CENTER_OFFSET, COLLISION_CENTER_OFFSET);
        player->setPos(basePos);

        // 防卡墙微调：尝试 8 个方向偏移
        if (tileMap->collidesWithWall(player->hitboxRect())) {
            const QVector<QPointF> offsets = {
                QPointF(0, -32), QPointF(0, 32),
                QPointF(-32, 0), QPointF(32, 0),
                QPointF(-32, -32), QPointF(32, -32),
                QPointF(-32, 32), QPointF(32, 32)
            };
            for (const QPointF &off : offsets) {
                player->setPos(basePos + off);
                if (!tileMap->collidesWithWall(player->hitboxRect())) {
                    return;
                }
            }
            // 所有偏移都失败，退回原始计算位置
            player->setPos(basePos);
        }
    };

    // 判断同地图还是跨地图
    if (portal.targetMap.isEmpty() || portal.targetMap == currentMapPath) {
        // ----------------- 同地图传送 -----------------
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                safeTeleportTo(p.rect.center());
                centerOn(player);
                break;
            }
        }
        // 传送后宠物重置
        if (pet) {
            qreal dist = QLineF(pet->pos(), player->pos()).length();
            if (dist > 160.0) pet->resetToOwner(player->pos());
        }
        // 恢复冷却
        QTimer::singleShot(2000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    } else {
        // ----------------- 跨地图传送 -----------------
        QString newMapPath = portal.targetMap;
        // 加载新地图，但不自动设置 start 点
        loadMap(newMapPath, false);
        bool found = false;
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                safeTeleportTo(p.rect.center());
                centerOn(player);
                found = true;
                break;
            }
        }
        if (!found) {
            // 备用：使用玩家起始点
            QPointF startPos = tileMap->getPlayerStart();
            if (!startPos.isNull()) {
                safeTeleportTo(startPos);
                centerOn(player);
            } else {
                qDebug() << "Warning: target portal not found, and no start point.";
            }
        }
        // 跨地图传送后宠物重置
        if (pet) pet->resetToOwner(player->pos());
        // 跨地图冷却稍长
        QTimer::singleShot(5000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    }
}

void Game::onPlayerDied()
{
    if (!player || !tileMap) return;

    // 重置玩家属性（等级、HP、MP等）
    player->reset();

    // 传送到当前地图的出生点
    QPointF startPos = tileMap->getPlayerStart();
    if (startPos.isNull()) {
        startPos = QPointF(100, 100);
    }
    player->setPos(startPos);

    // 重置传送冷却（避免死后立即传送造成bug）
    canTeleport = true;
    isTeleporting = false;

    // 摄像头重新对准
    centerOn(player);

    qDebug() << "Player died and respawned at start:" << startPos;
}

void Game::applyTerrainEffects()
{
    if (!player) return;

    QRectF playerRect = player->hitboxRect();

    // 火焰区域伤害
    bool onFire = false;
    for (const QRectF &rect : fireRects) {
        if (playerRect.intersects(rect)) {
            onFire = true;
            break;
        }
    }

    if (onFire) {
        fireDamageCounter++;
        if (fireDamageCounter >= FIRE_DAMAGE_INTERVAL) {
            fireDamageCounter = 0;
            player->takeDamage(5);   // 每次扣1血
            qDebug() << "Fireland damage! HP:" << player->getHp();
        }
    } else {
        fireDamageCounter = 0;   // 离开火焰重置计时器
    }
}