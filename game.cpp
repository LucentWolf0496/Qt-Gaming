#include "game.h"
#include "player.h"
#include "tilemap.h"
#include "spawner.h"
#include "pet.h"
#include <QDebug>
#include <QGraphicsRectItem>
#include <QFile>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QTextStream>
#include <QtMath>  // qSqrt
#include <QImageReader>
#include <QQueue>
#include <QSet>
#include <QPainter>

namespace {
    QVector<QPixmap> g_bombFrames;
    bool g_bombLoaded = false;
    QVector<QPixmap> g_fireBgFrames;
    bool g_fireBgLoaded = false;
}

QVector<QPixmap> g_daolangFrames;
bool g_daolangLoaded = false;

// 动态水帧缓存
static QVector<QPixmap> g_waterFrames;
static bool g_waterLoaded = false;

static void preloadWaterFrames() {
    if (g_waterLoaded) return;
    g_waterLoaded = true;
    for (int i = 0; i < 40; i++) {
        QString path = QString(":/images/water/%1.png").arg(i, 4, 10, QChar('0'));
        QPixmap pm(path);
        if (!pm.isNull()) {
            g_waterFrames.append(pm.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    qDebug() << "preloadWaterFrames: loaded" << g_waterFrames.size() << "frames";
}

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
    resize(800, 600);                      // 初始大小，可拖动缩放
    setMinimumSize(400, 300);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWindowTitle("Qt-Gaming");

    // 预加载 Projectile / Bomb / FireBg / Daolang / Monster 帧缓存
    preloadProjectileFrames();
    loadBombFrames();
    loadFireBgFrames();
    loadDaolangFrames();
    preloadMonsterFrames();
    preloadWaterFrames();

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
    if (hudKeyText) delete hudKeyText;
    if (minimapItem) { delete minimapItem; minimapItem = nullptr; }
    if (minimapDot)  { delete minimapDot;  minimapDot  = nullptr; }
    for (auto &d : diamonds) { delete d.item; }
    diamonds.clear();
    for (auto *s : portalSprites) { delete s; }
    portalSprites.clear();
    if (buffIndicator) { delete buffIndicator; buffIndicator = nullptr; }
    if (bgOverlay) { delete bgOverlay; bgOverlay = nullptr; }

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
    // ---------- 保存跨地图状态 ----------
    int savedLevel = 1, savedExp = 0, savedMaxExp = 100;
    int savedHp = 100, savedMaxHp = 100, savedMp = 100, savedMaxMp = 100;
    bool savedEnhanced = false;
    bool hasSavedState = false;

    if (player) {
        savedLevel = player->getLevel();
        savedExp = player->getExp();
        savedMaxExp = player->getMaxExp();
        savedHp = player->getHp();
        savedMaxHp = player->getMaxHp();
        savedMp = player->getMp();
        savedMaxMp = player->getMaxMp();
        savedEnhanced = player->getEnhanced();
        hasSavedState = true;

        if (player->scene()) scene->removeItem(player);
        delete player;
        player = nullptr;
        qDebug() << "[loadMap] Old player deleted (state saved: Lv." << savedLevel << ")";
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

    // 清理变身动画（跨地图时如果还在播，必须停掉）
    if (transformMovie) {
        transformMovie->stop();
        delete transformMovie;
        transformMovie = nullptr;
    }
    if (transformItem) {
        if (transformItem->scene()) transformItem->scene()->removeItem(transformItem);
        delete transformItem;
        transformItem = nullptr;
    }
    gamePaused = false;  // 解除变身动画的暂停状态
    stunTimer = 0;       // 清除定身状态
    animatedWaterTiles.clear();
    for (auto &p : petals) { if (p.item) delete p.item; }
    petals.clear();

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
    if (hudKeyText) { delete hudKeyText; hudKeyText = nullptr; }
    if (minimapItem) { delete minimapItem; minimapItem = nullptr; }
    if (minimapDot)  { delete minimapDot;  minimapDot  = nullptr; }
    for (auto &d : diamonds) { delete d.item; }
    diamonds.clear();
    attackBuffTimer = 0;
    for (auto *s : portalSprites) { delete s; }
    portalSprites.clear();
    if (buffIndicator) { delete buffIndicator; buffIndicator = nullptr; }
    if (bgOverlay) { delete bgOverlay; bgOverlay = nullptr; }

    // 清空可攻击对象列表
    hittableItems.clear();

    // 清空宝箱和门列表（旧列表中的 Tile 对象将在场景清理时自动删除）
    chests.clear();
    doors.clear();
    // 跨地图传送时宝箱和门重置，钥匙计数也重置
    keyCount = 0.0f;

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
                            // 只取 30%，减少地图固定怪物密度
                            if (QRandomGenerator::global()->bounded(100) >= 30) continue;
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
                    QVector<Tile*> waterTiles;
                    int waterCount = 0;
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;

                            // 随机选择水图片
                            QString waterPath;
                            bool isWeimingLake = mapFilePath.contains("Weiming_lake");
                            if (isWeimingLake) {
                                // Weiming_lake：三种水随机（60% / 35% / 5%）
                                int r = QRandomGenerator::global()->bounded(100);
                                if (r < 60) {
                                    waterPath = ":/images/water_d_1.png";
                                } else if (r < 95) {
                                    waterPath = ":/images/water_d_2.png";
                                } else {
                                    waterPath = ":/images/water_d_3.png";
                                }
                            } else {
                                // 其他地图：两种水随机（30% / 70%）
                                int r = QRandomGenerator::global()->bounded(100);
                                if (r < 30) {
                                    waterPath = ":/images/water_1.png";
                                } else {
                                    waterPath = ":/images/water_2.png";
                                }
                            }

                            Tile *waterTile = new Tile(waterPath, x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            waterTile->setZValue(-3);
                            scene->addItem(waterTile);
                            waterTiles.append(waterTile);
                            // Weiming_lake 的水用动态帧
                            if (isWeimingLake && !g_waterFrames.isEmpty()) {
                                animatedWaterTiles.append(waterTile);
                            }
                            waterCount++;
                        }
                    }
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

                            fireRects.append(QRectF(x * tileWidth, y * tileHeight, tileWidth, tileHeight));

                            // 随机选择火焰图片（fireland_1 / fireland_2 / fireland_3）
                            int r = QRandomGenerator::global()->bounded(3);
                            QString firePath = QString(":/images/fireland_%1.png").arg(r + 1);

                            Tile *fireTile = new Tile(firePath, x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            fireTile->setZValue(-4);  // 火焰与草地同层
                            scene->addItem(fireTile);
                            fireCount++;
                        }
                    }
                    qDebug() << "[ManualDraw] Fireland layer created" << fireCount << "tiles";
                    continue;
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

                        // 地板随机纹理（按地图区分）
                        if (layerName == "floor") {
                            if (mapFilePath.contains("school_map")) {
                                finalPath = ":/images/floor_room_1.png";
                            } else {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/floor_wood_1.png" : ":/images/floor_wood_2.png";
                            }
                        }
                        else if (layerName == "floor_onfire") {
                            if (mapFilePath.contains("Weiming_lake")) {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/floor_road_f_1.png" : ":/images/floor_road_f_2.png";
                            } else {
                                finalPath = ":/images/floor_road_f_1.png";
                            }
                        }
                        else if (layerName == "floor_road") {
                            if (mapFilePath.contains("Weiming_lake")) {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/floor_road_d_1.png" : ":/images/floor_road_d_2.png";
                            } else if (mapFilePath.contains("chamber1")) {
                                finalPath = ":/images/floor_road_3.png";
                            } else {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/floor_road_1.png" : ":/images/floor_road_2.png";
                            }
                        }
                        else if (layerName == "floor_room") {
                            if (mapFilePath.contains("Weiming_lake")) {
                                int r = QRandomGenerator::global()->bounded(100);
                                finalPath = (r < 90) ? ":/images/floor_room_d_1.png"
                                                      : ":/images/floor_room_d_2.png";
                            } else if (mapFilePath.contains("chamber1")) {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/floor_wood_3.png" : ":/images/floor_wood_4.png";
                            } else {
                                finalPath = ":/images/floor_room_1.png";
                            }
                        }
                        // 墙壁随机纹理（按地图区分 + 下方检测）
                        else if (layerName == "wall") {
                            bool hasWallBelow = false;
                            if (y + 1 < mapHeight) {
                                int belowRawGid = dataArr[(y + 1) * mapWidth + x].toInt();
                                int belowCleanGid = belowRawGid & 0x1FFFFFFF;
                                hasWallBelow = (belowCleanGid != 0);
                            }
                            bool isSchoolMap = mapFilePath.contains("school_map");
                            bool isChamber1 = mapFilePath.contains("chamber1");
                            if (!hasWallBelow) {
                                if (isSchoolMap) {
                                    finalPath = ":/images/wall_w_down.png";
                                } else if (isChamber1) {
                                    finalPath = ":/images/wall_r_down.png";
                                } else {
                                    finalPath = ":/images/wall_down.png";
                                }
                            } else {
                                if (isSchoolMap) {
                                    int r = QRandomGenerator::global()->bounded(2);
                                    finalPath = QString(":/images/wall_w_%1.png").arg(r + 1);
                                } else if (isChamber1) {
                                    int r = QRandomGenerator::global()->bounded(3);
                                    finalPath = QString(":/images/wall_r_%1.png").arg(r + 1);
                                } else {
                                    int r = QRandomGenerator::global()->bounded(3);
                                    finalPath = QString(":/images/wall_%1.png").arg(r + 1);
                                }
                            }
                        }
                        else if (layerName == "stair_h") {
                            finalPath = ":/images/stair_h.png";
                        }
                        else if (layerName == "stair_c1") {
                            finalPath = ":/images/stair_c1.png";
                        }
                        else if (layerName == "stair_c2") {
                            finalPath = ":/images/stair_c2.png";
                        }
                        else if (layerName == "grass" || layerName == "grassland") {
                            if (mapFilePath.contains("Weiming_lake")) {
                                int r = QRandomGenerator::global()->bounded(2);
                                finalPath = (r == 0) ? ":/images/grass_d_1.png" : ":/images/grass_d_2.png";
                            } else {
                                int r = QRandomGenerator::global()->bounded(100);
                                if (r < 85) {
                                    finalPath = ":/images/grass_1.png";
                                } else if (r < 92) {
                                    finalPath = ":/images/grass_2.png";
                                } else {
                                    finalPath = ":/images/grass_3.png";
                                }
                            }
                        }
                        // 宝箱图层：加入 chests 列表
                        else if (layerName == "chest") {
                            finalPath = layerImageMap.value("chest", ":/images/chest.png");
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            chests.append(tile);
                            tileCount++;
                            continue;
                        }
                        // 门图层：方向检测 + 加入 doors 列表 + 碰撞
                        else if (layerName == "door") {
                            bool hasLeftDoor = false, hasRightDoor = false, hasUpDoor = false, hasDownDoor = false;
                            if (x > 0) {
                                int leftGid = dataArr[y * mapWidth + (x - 1)].toInt() & 0x1FFFFFFF;
                                hasLeftDoor = (leftGid != 0);
                            }
                            if (x + 1 < mapWidth) {
                                int rightGid = dataArr[y * mapWidth + (x + 1)].toInt() & 0x1FFFFFFF;
                                hasRightDoor = (rightGid != 0);
                            }
                            if (y > 0) {
                                int upGid = dataArr[(y - 1) * mapWidth + x].toInt() & 0x1FFFFFFF;
                                hasUpDoor = (upGid != 0);
                            }
                            if (y + 1 < mapHeight) {
                                int downGid = dataArr[(y + 1) * mapWidth + x].toInt() & 0x1FFFFFFF;
                                hasDownDoor = (downGid != 0);
                            }
                            if (hasLeftDoor || hasRightDoor) {
                                finalPath = ":/images/door_h.png";
                            } else if (hasUpDoor || hasDownDoor) {
                                finalPath = ":/images/door_c.png";
                            } else {
                                finalPath = ":/images/door_h.png";
                            }
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            doors.append(tile);
                            tileMap->addWallTile(tile);
                            tileCount++;
                            continue;
                        }
                        // 传送门图层：跳过（用 objectgroup rect 的漩涡替代）
                        else if (layerName == "portal" || layerName == "portal_image") {
                            tileCount++;
                            continue;
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

                        // ========== 渲染层级（Z值，全部负数，确保游戏对象默认 Z=0 在地形之上）==========
                        // 草(-4) → 水(-3) → 路(-2) → 墙(-1) → 装饰(0)
                        if (layerName == "grass" || layerName == "grassland") {
                            tile->setZValue(-4);
                        } else if (layerName == "floor" || layerName == "floor_road" || layerName == "floor_room" || layerName == "floor_onfire") {
                            tile->setZValue(-2);
                        } else if (layerName == "wall" || layerName == "door" || layerName == "stair_h" || layerName == "stair_c1" || layerName == "stair_c2") {
                            tile->setZValue(-1);
                        } else {
                            tile->setZValue(0);
                        }

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

            // ===== 传送门替换：用 objectgroup 的 rect 创建旋转 shikongxuanwo.png =====
            for (const Portal &p : tileMap->getPortals()) {
                int size = qMax((int)p.rect.width(), (int)p.rect.height());
                auto *sprite = new QGraphicsPixmapItem();
                QPixmap pm(":/images/shikongxuanwo.png");
                pm = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                sprite->setPixmap(pm);
                sprite->setTransformOriginPoint(size/2.0, size/2.0);
                sprite->setPos(p.rect.x(), p.rect.y());
                sprite->setZValue(4);
                sprite->setTransformationMode(Qt::SmoothTransformation);
                scene->addItem(sprite);
                portalSprites.append(sprite);
            }
            qDebug() << "[Portal] Created" << portalSprites.size() << "rotating portal sprites from objectgroup";
        } else {
            qDebug() << "[ManualDraw] Failed to parse JSON for manual drawing:" << mapFilePath;
        }
        file.close();
    } else {
        qDebug() << "[ManualDraw] Cannot open map file for manual drawing:" << mapFilePath;
    }

    // ---------- 3.5 主地图背景叠加（school_map 专用）----------
    if (mapFilePath.contains("school_map")) {
        QPixmap left(":/images/try_background_left.png");
        QPixmap right(":/images/try_background_right.png");
        if (!left.isNull() && !right.isNull()) {
            int totalW = left.width() + right.width();
            int totalH = qMax(left.height(), right.height());
            QPixmap stitched(totalW, totalH);
            stitched.fill(Qt::transparent);
            QPainter p(&stitched);
            p.drawPixmap(0, 0, left);
            p.drawPixmap(left.width(), 0, right);
            p.end();

            int mapW = tileMap->getMapWidth() * tileMap->getTileWidth();
            int mapH = tileMap->getMapHeight() * tileMap->getTileHeight();
            QPixmap scaled = stitched.scaled(mapW, mapH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            bgOverlay = new QGraphicsPixmapItem();
            bgOverlay->setPixmap(scaled);
            bgOverlay->setZValue(-0.5);  // 在地板(-2)/墙(-1)之上，装饰(0)之下
            bgOverlay->setTransformationMode(Qt::SmoothTransformation);
            bgOverlay->setCacheMode(QGraphicsItem::DeviceCoordinateCache);  // 缩放时重新渲染保持清晰
            scene->addItem(bgOverlay);
            qDebug() << "[loadMap] Background overlay created:" << mapW << "x" << mapH;
        }
    }

    // ---------- 4. 创建玩家 ----------
    player = new Player(tileMap);
    scene->addItem(player);
    connect(player, &Player::died, this, &Game::onPlayerDied);   // 连接死亡信号

    // 跨地图：恢复玩家等级/HP/MP/形态
    if (hasSavedState) {
        player->restoreState(savedLevel, savedExp, savedMaxExp,
                             savedHp, savedMaxHp, savedMp, savedMaxMp,
                             savedEnhanced);
        // 恢复已解锁的被动效果
        if (savedLevel >= 5) {
            explosionsEnabled = true;
        }
    }

    qDebug() << "[loadMap] Player created.";

    // ---------- 5. 创建背后火焰（2级+才显示）----------
    if (!fireBgItem && !g_fireBgFrames.isEmpty() && player && player->getLevel() >= 2) {
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

    // ---------- 7. 创建巢穴（基于玩家出生点 + 边界检查）----------
    QPointF spawnBase = playerStart;
    if (spawnBase.isNull()) spawnBase = QPointF(100, 100);
    int ts = tileMap->getTileWidth();
    int mapW = tileMap->getMapWidth() * ts;
    int mapH = tileMap->getMapHeight() * ts;

    // 安全放置函数：夹在可行走区域内
    auto safeSpawn = [&](QPointF pos) -> QPointF {
        pos.setX(qBound(ts * 2.0, pos.x(), mapW - ts * 3.0));
        pos.setY(qBound(ts * 2.0, pos.y(), mapH - ts * 3.0));
        // 尝试偏移直到不撞墙/水
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                QPointF test(pos.x() + dx * ts, pos.y() + dy * ts);
                QRectF tr(test.x(), test.y(), 40, 40);
                if (!tileMap->collidesWithWall(tr) && !tileMap->collidesWithWater(tr))
                    return test;
            }
        }
        return pos;  // 实在找不到就返回夹紧位置
    };

    QPointF offsets[] = {{476, 100}, {76, 300}};
    for (auto &off : offsets) {
        QPointF sp = safeSpawn(spawnBase + QPointF(off.x(), off.y()));
        spawners.append(new Spawner(tileMap, scene, sp, this));
    }
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
        QTimer::singleShot(100, this, [this]() {
            if (player) spawnArrivalEffect(player->sceneBoundingRect().center());
        });
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

    // 创建小地图
    QTimer::singleShot(100, this, [this]() { createMinimap(); });

    // 生成钻石
    spawnDiamonds();

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
    // ========== 管理员模式：czz 切换 ==========
    int key = event->key();
    if (!adminMode) {
        // 收集 czz 序列
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            keyBuffer += QChar(key).toLower();
            if (keyBuffer.size() > 20) keyBuffer = keyBuffer.right(20);
            if (keyBuffer.endsWith("czz")) {
                adminMode = true;
                keyBuffer.clear();
                qDebug() << "=== ADMIN MODE ON ===";
                return;
            }
        }
    } else {
        // 管理员模式下处理命令
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            processAdminKey(key);
            return;
        }
        if (key == Qt::Key_X) {
            processAdminKey(key);
            return;
        }
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            keyBuffer += QChar(key).toLower();
            if (keyBuffer.endsWith("czz")) {
                adminMode = false;
                keyBuffer.clear();
                qDebug() << "=== ADMIN MODE OFF ===";
                return;
            }
        }
    }

    switch (event->key()) {
    case Qt::Key_W: upPressed = true; break;
    case Qt::Key_S: downPressed = true; break;
    case Qt::Key_A: leftPressed = true; break;
    case Qt::Key_D: rightPressed = true; break;
    case Qt::Key_I: if (!isNearPortal()) skillMeteorBurst(); break;
    case Qt::Key_H: if (!isNearPortal()) skillTriangleShot(); break;
    case Qt::Key_N: if (!isNearPortal()) skillBlueBurst(); break;
    case Qt::Key_J: if (!isNearPortal()) skillNormalAttack(); break;
    case Qt::Key_K: if (!isNearPortal()) skillFlashBlade(); break;
    case Qt::Key_L: if (!isNearPortal()) skillShieldActivate(); break;
    case Qt::Key_O:
        if (speedCooldownTimer > 0 || speedBoostTimer > 0) break;
        if (!player) break;
        player->setSpeed(8.0);
        speedBoostTimer = SPEED_BOOST_DURATION;
        speedCooldownTimer = SPEED_COOLDOWN + SPEED_BOOST_DURATION;
        qDebug() << "Speed boost ON for 5s";
        break;
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
    case Qt::Key_F11:
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
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
    // 地图切换期间玩家/地图可能为空
    if (!player || !tileMap || !scene) return;

    // ========== 受伤定身倒计时 ==========
    if (stunTimer > 0) stunTimer--;

    // ========== 加速技能冷却 ==========
    if (speedBoostTimer > 0) {
        speedBoostTimer--;
        if (speedBoostTimer == 0 && player) player->setSpeed(4.0);  // 恢复原速
    }
    if (speedCooldownTimer > 0) speedCooldownTimer--;

    // ========== 动态水帧切换 ==========
    if (!animatedWaterTiles.isEmpty() && !g_waterFrames.isEmpty()) {
        waterFrameTick++;
        if (waterFrameTick >= 3) {
            waterFrameTick = 0;
            waterFrameIdx = (waterFrameIdx + 1) % g_waterFrames.size();
            const QPixmap &wf = g_waterFrames[waterFrameIdx];
            for (Tile *t : animatedWaterTiles) {
                qreal sx = 32.0 / wf.width();
                qreal sy = 32.0 / wf.height();
                t->setTransform(QTransform::fromScale(sx, sy));
                t->setPixmap(wf);
            }
        }
    }

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
            int damage = getBuffedDamage(40);
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
        QPointF oldPos = player->pos();
        // 定身期间禁止移动
        if (stunTimer > 0) {
            player->move(false, false, false, false);
        } else {
            player->move(upPressed, downPressed, leftPressed, rightPressed);
        }
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

    // 更新背后火焰动画和位置（2级+才显示）
    if (fireBgItem && player && player->getLevel() >= 2 && !g_fireBgFrames.isEmpty()) {
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
    // ========== 传送门旋转 ==========
    portalRotTick++;
    if (portalRotTick >= 12) {  // 每 12 帧旋转 90°
        portalRotTick = 0;
        for (auto *s : portalSprites) {
            if (s) s->setRotation(s->rotation() + 90);
        }
    }

    checkPortal();
    checkInteractions();
    updateMinimap();            // ← 更新小地图红点位置
    updateDiamonds();           // ← 钻石动画与碰撞
    updatePetals();             // ← 梅花瓣粒子
    // 紫钻 buff：更新头顶十字位置，到期移除
    if (attackBuffTimer > 0) {
        attackBuffTimer--;
        if (buffIndicator && player) {
            QPointF pc = player->sceneBoundingRect().center();
            buffIndicator->setPos(pc.x() - 8, pc.y() - 40);
        }
        if (attackBuffTimer == 0 && buffIndicator) {
            if (buffIndicator->scene()) buffIndicator->scene()->removeItem(buffIndicator);
            delete buffIndicator;
            buffIndicator = nullptr;
        }
    }
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

    // I技能：不移动时才能使用（先检查方向，再扣蓝）
    if (upPressed || downPressed || leftPressed || rightPressed) return;
    if (!player->consumeMp(10)) return; // 消耗 10 MP，不足则无法释放

    // 变身形态下播放飞火施法动画（间隔1帧，更快）
    if (player->getEnhanced()) {
        player->playCastAnimation(":/images/player_enhanced_fly_fire.gif", 1);
    }

    // 以玩家中心为发射原点
    QPointF center = player->sceneBoundingRect().center();

    qreal speed = 8.0;       // 火炮飞行速度（像素/帧）
    int damage = getBuffedDamage(25);  // 伤害值（紫钻翻倍）
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
    int damage = getBuffedDamage(15);  // 伤害（紫钻翻倍）

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
    int damage = getBuffedDamage(45); // J技能伤害15的3倍（紫钻翻倍）

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
    if (flashState.active) return; // 闪现中不能再次使用

    // ========== 静止时按 K：回血技能（红色+字上升消失）==========
    if (!upPressed && !downPressed && !leftPressed && !rightPressed) {
        if (!player->consumeMp(15)) return;
        player->recoverHpMp(30, 0);
        qDebug() << "K-heal: recovered 30 HP";

        // 在玩家位置生成 5 个红色"+"字，缓缓上升并消失
        QPointF center = player->sceneBoundingRect().center();
        for (int i = 0; i < 5; i++) {
            auto *cross = new QGraphicsSimpleTextItem("+");
            cross->setBrush(QBrush(QColor(255, 50, 50)));
            QFont f = cross->font();
            f.setPointSize(14 + QRandomGenerator::global()->bounded(8));
            f.setBold(true);
            cross->setFont(f);
            cross->setZValue(50);
            // 随机散布在玩家周围 40px 范围内
            qreal ox = QRandomGenerator::global()->bounded(40) - 20;
            qreal oy = QRandomGenerator::global()->bounded(20) - 10;
            cross->setPos(center.x() + ox - 8, center.y() + oy - 16);
            scene->addItem(cross);

            // 动画：上升 + 淡出
            int duration = 800 + QRandomGenerator::global()->bounded(400); // 0.8~1.2秒
            int steps = 20;
            int interval = duration / steps;
            qreal startY = cross->y();
            auto *timer = new QTimer(this);
            int *step = new int(0);
            connect(timer, &QTimer::timeout, [timer, cross, step, startY, steps]() {
                (*step)++;
                qreal t = (qreal)(*step) / steps;
                cross->setY(startY - t * 60);  // 上升 60px
                QColor c(255, 50, 50);
                c.setAlpha(255 * (1.0 - t));    // 淡出
                cross->setBrush(QBrush(c));
                if (*step >= steps) {
                    timer->stop();
                    if (cross->scene()) cross->scene()->removeItem(cross);
                    delete cross;
                    delete step;
                    timer->deleteLater();
                }
            });
            timer->start(interval);
        }
        return;
    }

    // ========== 有方向时：闪现斩 ==========
    if (!player->consumeMp(15)) return; // 消耗 15 MP，不足则无法释放

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
        if (!adminMode && tileMap->collidesWithWall(player->hitboxRect())) {
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
                enemy->takeDamage(getBuffedDamage(15));
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
    shieldItem->setZValue(30);  // 在玩家（Z=2）和敌人（Z=5~6）之上
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

    // 钥匙数量显示（金色）
    hudKeyText = new QGraphicsSimpleTextItem();
    hudKeyText->setBrush(QBrush(QColor(255, 215, 0)));
    QFont keyFont = hudKeyText->font();
    keyFont.setPointSize(12);
    keyFont.setBold(true);
    hudKeyText->setFont(keyFont);
    scene->addItem(hudKeyText);
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

    if (hudKeyText) {
        hudKeyText->setText(QString("🔑 Keys: %1").arg(keyCount));
        hudKeyText->setPos(hudPos.x() + 2, hudPos.y() + 80); // 放在 HP 条下方
        hudKeyText->setZValue(1002);
    }
}

void Game::updateKeyDisplay()
{
    if (hudKeyText) {
        hudKeyText->setText(QString("🔑 Keys: %1").arg(keyCount, 0, 'f', 2));
    }
}

void Game::createMinimap()
{
    if (!scene || !tileMap) return;

    int mapW = tileMap->getMapWidth() * tileMap->getTileWidth();
    int mapH = tileMap->getMapHeight() * tileMap->getTileHeight();
    if (mapW <= 0 || mapH <= 0) return;

    // 小地图尺寸
    qreal mmW = 150;
    qreal scale = mmW / mapW;
    qreal mmH = mapH * scale;

    // 渲染场景到小 pixmap
    QPixmap pixmap(mmW, mmH);
    pixmap.fill(QColor(0, 0, 0, 180));
    QPainter painter(&pixmap);
    scene->render(&painter, QRectF(0, 0, mmW, mmH), QRectF(0, 0, mapW, mapH));
    painter.end();

    minimapItem = new QGraphicsPixmapItem();
    minimapItem->setPixmap(pixmap);
    minimapItem->setZValue(1000);
    minimapItem->setOpacity(0.75);
    scene->addItem(minimapItem);

    // 玩家红点
    minimapDot = new QGraphicsEllipseItem(-3, -3, 6, 6);
    minimapDot->setBrush(QBrush(Qt::red));
    minimapDot->setPen(QPen(Qt::white, 1));
    minimapDot->setZValue(1001);
    scene->addItem(minimapDot);
}

void Game::updateMinimap()
{
    if (!minimapItem || !minimapDot || !player || !tileMap) return;

    // 小地图固定在视口右上角
    int vpW = viewport()->width();
    QPointF mmPos = mapToScene(vpW - 160, 10);
    minimapItem->setPos(mmPos);

    // 红点 = 玩家位置按比例缩放
    int mapW = tileMap->getMapWidth() * tileMap->getTileWidth();
    int mapH = tileMap->getMapHeight() * tileMap->getTileHeight();
    if (mapW <= 0 || mapH <= 0) return;
    qreal scale = 150.0 / mapW;

    QPointF pc = player->sceneBoundingRect().center();
    minimapDot->setPos(mmPos.x() + pc.x() * scale,
                       mmPos.y() + pc.y() * scale);
}

void Game::spawnDiamonds()
{
    if (!tileMap || !scene) return;
    int mw = tileMap->getMapWidth();
    int mh = tileMap->getMapHeight();
    int ts = tileMap->getTileWidth();
    if (mw <= 0 || mh <= 0) return;

    QStringList imgPaths = {
        ":/images/red_diamond.png",
        ":/images/blue_diamond.png",
        ":/images/purple_diamond.png"
    };
    int count = qMin(18, mw * mh / 200);  // 大约每200个tile放1个钻石

    for (int n = 0; n < count; n++) {
        // 随机尝试找可通行位置
        for (int attempt = 0; attempt < 20; attempt++) {
            int gx = QRandomGenerator::global()->bounded(2, mw - 2);
            int gy = QRandomGenerator::global()->bounded(2, mh - 2);
            QRectF testRect(gx * ts, gy * ts, ts, ts);
            if (!tileMap->collidesWithWall(testRect) && !tileMap->collidesWithWater(testRect)) {
                int type = QRandomGenerator::global()->bounded(3);
                auto *item = new QGraphicsPixmapItem();
                QPixmap pm(imgPaths[type]);
                qreal s = (qreal)ts / pm.width();
                item->setTransform(QTransform::fromScale(s, s));
                item->setPixmap(pm);
                item->setPos(gx * ts, gy * ts);
                item->setZValue(6);
                item->setTransformationMode(Qt::SmoothTransformation);
                scene->addItem(item);
                diamonds.append({item, type});
                break;
            }
        }
    }
    qDebug() << "[spawnDiamonds] placed" << diamonds.size() << "diamonds";
}

void Game::updateDiamonds()
{
    if (!player) return;
    QRectF pr = player->hitboxRect();

    for (int i = diamonds.size() - 1; i >= 0; --i) {
        auto &d = diamonds[i];
        if (!d.item) continue;

        // 碰撞检测
        if (pr.intersects(d.item->sceneBoundingRect())) {
            QPointF dc = d.item->sceneBoundingRect().center();
            int type = d.type;

            // 移除钻石
            scene->removeItem(d.item);
            delete d.item;
            diamonds.removeAt(i);

            if (type == 0) {
                // 红钻石：补血 25 + 红十字特效
                player->recoverHpMp(25, 0);
                spawnCrossEffect(dc, 0);  // 红色十字
            } else if (type == 1) {
                // 蓝钻石：补蓝 25 + 蓝十字特效
                player->recoverHpMp(0, 25);
                spawnCrossEffect(dc, 1);  // 蓝色十字
            } else {
                // 紫钻石：攻击翻倍 10s + 头顶固定紫色十字
                attackBuffTimer = ATTACK_BUFF_DURATION;
                if (buffIndicator) { delete buffIndicator; }
                buffIndicator = new QGraphicsSimpleTextItem("+");
                buffIndicator->setBrush(QBrush(QColor(180, 60, 255)));
                QFont f = buffIndicator->font();
                f.setPointSize(20); f.setBold(true);
                buffIndicator->setFont(f);
                buffIndicator->setZValue(50);
                scene->addItem(buffIndicator);
            }
        }
    }
}

void Game::spawnCrossEffect(QPointF center, int colorType)
{
    // colorType: 0=红, 1=蓝, 2=紫
    QColor colors[] = {
        QColor(255, 50, 50),    // 红
        QColor(50, 120, 255),   // 蓝
        QColor(180, 60, 255)    // 紫
    };
    QColor c = colors[colorType];

    for (int i = 0; i < 5; i++) {
        auto *cross = new QGraphicsSimpleTextItem("+");
        cross->setBrush(QBrush(c));
        QFont f = cross->font();
        f.setPointSize(14 + QRandomGenerator::global()->bounded(8));
        f.setBold(true);
        cross->setFont(f);
        cross->setZValue(50);
        qreal ox = QRandomGenerator::global()->bounded(40) - 20;
        cross->setPos(center.x() + ox - 8, center.y() - 16);
        scene->addItem(cross);

        int duration = 800 + QRandomGenerator::global()->bounded(400);
        int steps = 20;
        int interval = duration / steps;
        qreal startY = cross->y();
        auto *timer = new QTimer(this);
        int *step = new int(0);
        connect(timer, &QTimer::timeout, [timer, cross, step, startY, steps, c]() {
            (*step)++;
            qreal t = (qreal)(*step) / steps;
            cross->setY(startY - t * 60);
            QColor fc = c;
            fc.setAlpha(255 * (1.0 - t));
            cross->setBrush(QBrush(fc));
            if (*step >= steps) {
                timer->stop();
                if (cross->scene()) cross->scene()->removeItem(cross);
                delete cross;
                delete step;
                timer->deleteLater();
            }
        });
        timer->start(interval);
    }
}

void Game::spawnArrivalEffect(QPointF center)
{
    // 红白交错竖线激光，从上向下闪过
    for (int i = 0; i < 10; i++) {
        QColor c = (i % 2 == 0) ? QColor(255, 60, 60) : QColor(255, 255, 255);
        qreal ox = QRandomGenerator::global()->bounded(70) - 35;
        qreal h = 80 + QRandomGenerator::global()->bounded(100);  // 80~180px
        qreal w = 2 + QRandomGenerator::global()->bounded(4);
        auto *line = new QGraphicsRectItem(-w/2, 0, w, h);
        line->setBrush(QBrush(c));
        line->setPen(Qt::NoPen);
        line->setPos(center.x() + ox, center.y() - 30);
        line->setZValue(50);
        scene->addItem(line);

        int duration = 700 + QRandomGenerator::global()->bounded(500); // 0.7~1.2s
        int steps = 20;
        int interval = duration / steps;
        auto *timer = new QTimer(this);
        int *step = new int(0);
        qreal startY = line->y();
        connect(timer, &QTimer::timeout, [timer, line, step, startY, steps]() {
            (*step)++;
            qreal t = (qreal)(*step) / steps;
            line->setY(startY + t * 40);  // 向下移动
            QColor c = line->brush().color();
            c.setAlpha(255 * (1.0 - t));
            line->setBrush(QBrush(c));
            qreal h = line->rect().height();
            line->setRect(-line->rect().width()/2, 0, line->rect().width(), h * (1.0 - t * 0.7));
            if (*step >= steps) {
                timer->stop();
                if (line->scene()) line->scene()->removeItem(line);
                delete line;
                delete step;
                timer->deleteLater();
            }
        });
        timer->start(interval);
    }
}

void Game::updatePetals()
{
    if (!scene || !player) return;

    // 每帧从右上生成，暴雪密度
    static int spawnTick = 0;
    spawnTick++;
    if (spawnTick >= 1 && petals.size() < 200) {
        spawnTick = 0;
        QPointF vpTopRight = mapToScene(viewport()->width(), 0);
        qreal vpH = mapToScene(0, viewport()->height()).y() - mapToScene(0, 0).y();

        // 随机梅花或叶子
        bool isFlower = (QRandomGenerator::global()->bounded(2) == 0);
        auto *leaf = new QGraphicsEllipseItem(-2, -5, 4, 10);
        if (isFlower) {
            // 梅花瓣：粉白
            leaf->setBrush(QBrush(QColor(255, 180 + QRandomGenerator::global()->bounded(60), 200 + QRandomGenerator::global()->bounded(40), 200)));
            leaf->setPen(QPen(QColor(255, 150, 180, 150), 1));
        } else {
            // 叶子：亮绿/黄绿
            int g = 180 + QRandomGenerator::global()->bounded(75);
            leaf->setBrush(QBrush(QColor(60, g, 30, 200)));
            leaf->setPen(QPen(QColor(80, g-20, 40, 140), 1));
        }
        // 从右上方随机位置出现
        qreal startX = vpTopRight.x() - 20 + QRandomGenerator::global()->bounded(40);
        qreal startY = mapToScene(0, 0).y() - QRandomGenerator::global()->bounded((int)vpH * 0.4);
        leaf->setPos(startX, startY);
        leaf->setZValue(10000);
        leaf->setTransformOriginPoint(2, 5);
        scene->addItem(leaf);

        Petal p;
        p.item = leaf;
        p.vx = -(0.3 + QRandomGenerator::global()->bounded(15) / 10.0);  // 向左飘 -0.3~-1.8
        p.vy = 0.3 + QRandomGenerator::global()->bounded(12) / 10.0;     // 向下 0.3~1.5
        p.rotation = QRandomGenerator::global()->bounded(360);
        p.life = 500 + QRandomGenerator::global()->bounded(300);  // 8~13秒
        petals.append(p);
    }

    // 更新所有落叶
    QPointF vpBotLeft = mapToScene(0, viewport()->height());
    for (int i = petals.size() - 1; i >= 0; --i) {
        Petal &p = petals[i];
        p.life--;
        p.rotation += 0.8;  // 稍快旋转
        p.vx += qCos(p.rotation * M_PI / 180.0) * 0.015;  // 左右摇摆

        qreal nx = p.item->x() + p.vx;
        qreal ny = p.item->y() + p.vy;
        p.item->setPos(nx, ny);
        p.item->setRotation(p.rotation);

        // 仅在最后 20 帧快速淡出
        if (p.life < 20) {
            QColor c = p.item->brush().color();
            c.setAlpha(c.alpha() * p.life / 20);
            p.item->setBrush(QBrush(c));
        }

        // 超出左下屏幕或寿命结束
        if (p.life <= 0 || nx < vpBotLeft.x() - 40 || ny > vpBotLeft.y() + 40) {
            scene->removeItem(p.item);
            delete p.item;
            petals.removeAt(i);
        }
    }
}

void Game::processAdminKey(int key)
{
    static int pendingX = 0;
    if (!player || !tileMap) return;

    if (key == Qt::Key_X) { pendingX = 1; qDebug() << "Admin: x pressed, waiting for num"; return; }

    int num = key - Qt::Key_0;
    if (pendingX == 1) {
        pendingX = 0;
        if (num == 0) { loadMap(":/maps/school_map.tmj", true); qDebug() << "Admin: → school_map spawn"; }
        else if (num == 1) { loadMap(":/maps/chamber1.tmj", true); qDebug() << "Admin: → chamber1"; }
        else if (num == 2) { loadMap(":/maps/lianda.tmj", true); qDebug() << "Admin: → lianda"; }
        else if (num == 3) { loadMap(":/maps/Weiming_lake.tmj", true); qDebug() << "Admin: → Weiming_lake"; }
        return;
    }

    if (num >= 1 && num <= 9) {
        player->setLevel(num);
        if (num >= 3) player->setEnhanced(true); else player->setEnhanced(false);
        explosionsEnabled = (num >= 5);
        qDebug() << "Admin: level set to" << num;
    }
}

bool Game::isNearPortal() const
{
    if (!player || !tileMap) return false;
    int ts = tileMap->getTileWidth();  // 32
    QRectF nearRect = player->hitboxRect().adjusted(-ts * 2, -ts * 2, ts * 2, ts * 2);
    for (const Portal &p : tileMap->getPortals()) {
        if (nearRect.intersects(p.rect)) return true;
    }
    return false;
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
    if (newLevel == 2) {
        // 2级：开启背后火焰
        if (!fireBgItem && !g_fireBgFrames.isEmpty()) {
            fireBgItem = new QGraphicsPixmapItem();
            scene->addItem(fireBgItem);
            fireBgItem->setTransformationMode(Qt::SmoothTransformation);
            fireBgItem->setZValue(1);
            fireBgItem->setPixmap(g_fireBgFrames[0]);
        }
    }
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
    transformItem->setZValue(999999);  // 绝对最上层
    scene->addItem(transformItem);

    // 以玩家为中心显示（场景坐标）
    QPointF pc = player->sceneBoundingRect().center();
    transformItem->setPos(pc.x() - 400, pc.y() - 388);

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
                stunTimer = STUN_DURATION;  // 定身 0.2s
                qDebug() << "Player hit by enemy! Damage:" << ep->getDamage() << "Stunned.";

                // 生成 4 个紫色"-"号旋转上升
                QPointF pc = player->sceneBoundingRect().center();
                for (int j = 0; j < 4; j++) {
                    auto *minus = new QGraphicsSimpleTextItem("-");
                    minus->setBrush(QBrush(QColor(180, 60, 255)));
                    QFont f = minus->font();
                    f.setPointSize(16 + QRandomGenerator::global()->bounded(6));
                    f.setBold(true);
                    minus->setFont(f);
                    minus->setZValue(50);
                    qreal ox = QRandomGenerator::global()->bounded(30) - 15;
                    qreal oy = QRandomGenerator::global()->bounded(20) - 40;
                    minus->setPos(pc.x() + ox, pc.y() + oy);
                    scene->addItem(minus);

                    int duration = 600 + QRandomGenerator::global()->bounded(300);
                    int steps = 15;
                    int interval = duration / steps;
                    auto *t = new QTimer(this);
                    int *step = new int(0);
                    qreal startX = minus->x(), startY = minus->y();
                    connect(t, &QTimer::timeout, [t, minus, step, startX, startY, steps]() {
                        (*step)++;
                        qreal r = (qreal)(*step) / steps;
                        minus->setY(startY - r * 50);  // 上升
                        // 左右摆动模拟旋转
                        minus->setX(startX + qSin(r * M_PI * 3) * 15);
                        QColor c(180, 60, 255);
                        c.setAlpha(255 * (1.0 - r));
                        minus->setBrush(QBrush(c));
                        if (*step >= steps) {
                            t->stop();
                            if (minus->scene()) minus->scene()->removeItem(minus);
                            delete minus;
                            delete step;
                            t->deleteLater();
                        }
                    });
                    t->start(interval);
                }
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
        // 传送到达特效
        spawnArrivalEffect(player->sceneBoundingRect().center());
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
        // 传送到达特效
        spawnArrivalEffect(player->sceneBoundingRect().center());
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

void Game::checkInteractions()
{
    if (!player) return;
    QRectF playerRect = player->hitboxRect();   // 已在开头定义

    // 检测宝箱
    for (int i = chests.size() - 1; i >= 0; --i) {
        Tile *chest = chests[i];
        if (playerRect.intersects(chest->sceneBoundingRect())) {
            openChest(chest);
            chests.removeAt(i);
            break;
        }
    }

    // 检测门（BFS 整片移除）
    for (int i = doors.size() - 1; i >= 0; --i) {
        Tile *door = doors[i];
        QRectF doorRect = door->sceneBoundingRect();
        int expand = tileMap->getTileWidth();  // 32
        QRectF extendedRect = doorRect.adjusted(-expand, -expand, expand, expand);
        // 直接使用外层的 playerRect，不要再定义新的
        if (playerRect.intersects(extendedRect)) {
            if (keyCount > 0) {
                int removed = removeDoorRegion(door);
                if (removed > 0) {
                    keyCount -= 1.0f;
                    updateKeyDisplay();
                    qDebug() << "Door region opened! Keys left:" << keyCount;
                    // 开门特效
                    QPointF regionCenter = doorRect.center();
                    QGraphicsEllipseItem *effect = new QGraphicsEllipseItem(-30, -30, 60, 60);
                    effect->setBrush(QBrush(QColor(0, 255, 0, 150)));
                    effect->setPen(Qt::NoPen);
                    effect->setPos(regionCenter);
                    scene->addItem(effect);
                    QTimer::singleShot(200, [effect]() {
                        if (effect->scene()) effect->scene()->removeItem(effect);
                        delete effect;
                    });
                }
                break;
            } else {  }
        }
    }
}

void Game::openChest(Tile *chest)
{
    keyCount += 0.25f;
    qDebug() << "Chest opened! Keys:" << keyCount;

    // 可选：播放简易开箱特效（金色闪光）
    QPointF center = chest->sceneBoundingRect().center();
    QGraphicsEllipseItem *effect = new QGraphicsEllipseItem(-15, -15, 30, 30);
    effect->setBrush(QBrush(QColor(255, 215, 0, 200)));
    effect->setPen(Qt::NoPen);
    effect->setPos(center);
    scene->addItem(effect);
    QTimer::singleShot(200, [effect]() {
        if (effect->scene()) effect->scene()->removeItem(effect);
        delete effect;
    });

    // 移除宝箱
    scene->removeItem(chest);
    delete chest;

    updateKeyDisplay();
}

int Game::removeDoorRegion(Tile *startDoor)
{
    if (!startDoor || !tileMap) return 0;

    int tileW = tileMap->getTileWidth();
    int tileH = tileMap->getTileHeight();

    // BFS 队列
    QQueue<Tile*> queue;
    QSet<Tile*> visited;

    queue.enqueue(startDoor);
    visited.insert(startDoor);

    while (!queue.isEmpty()) {
        Tile *current = queue.dequeue();

        // 获取当前瓦片的位置（格子坐标）
        QPointF pos = current->pos();
        int gx = qRound(pos.x() / tileW);
        int gy = qRound(pos.y() / tileH);

        // 检查四个方向的邻居
        QVector<QPoint> dirs = { QPoint(1,0), QPoint(-1,0), QPoint(0,1), QPoint(0,-1) };
        for (const QPoint &d : dirs) {
            int nx = gx + d.x();
            int ny = gy + d.y();
            QPointF neighborPos(nx * tileW, ny * tileH);

            // 在 doors 列表中查找相同位置的瓦片
            for (Tile *door : doors) {
                if (visited.contains(door)) continue;
                if (door->pos() == neighborPos) {
                    visited.insert(door);
                    queue.enqueue(door);
                    break;
                }
            }
        }
    }

    // 删除所有连通的门瓦片
    int removedCount = 0;
    for (Tile *door : visited) {
        // 从碰撞系统中移除
        tileMap->removeWallTile(door);
        // 从场景中移除并删除
        scene->removeItem(door);
        delete door;
        removedCount++;
    }

    // 从 doors 列表中移除这些瓦片
    for (Tile *door : visited) {
        doors.removeAll(door);
    }

    qDebug() << "Removed" << removedCount << "connected door tiles with one key.";
    return removedCount;
}