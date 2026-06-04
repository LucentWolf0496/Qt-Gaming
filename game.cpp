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
#include <QtMath>
#include <QImageReader>
#include <QQueue>
#include <QSet>
#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QTextEdit>

namespace {
    QVector<QPixmap> g_bombFrames;
    bool g_bombLoaded = false;
    QVector<QPixmap> g_fireBgFrames;
    bool g_fireBgLoaded = false;
    QVector<QPixmap> g_fireHitFrames;
    bool g_fireHitLoaded = false;
    QVector<QPixmap> g_rushJumbFrames;
    bool g_rushJumbLoaded = false;
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
                        144, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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

    void loadFireHitFrames()
    {
        if (g_fireHitLoaded) return;
        g_fireHitLoaded = true;
        QImageReader reader(":/images/fire_hit.gif");
        reader.setAutoDetectImageFormat(true);
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                g_fireHitFrames.append(QPixmap::fromImage(img).scaled(
                    144, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (g_fireHitFrames.isEmpty()) {
            qDebug() << "Failed to load fire_hit.gif frames";
        }
    }

    void loadRushJumbFrames()
    {
        if (g_rushJumbLoaded) return;
        g_rushJumbLoaded = true;
        QImageReader reader(":/images/player_enhanced_k_rush_jumb_bright.gif");
        reader.setAutoDetectImageFormat(true);
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                g_rushJumbFrames.append(QPixmap::fromImage(img).scaled(
                    77, 77, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (g_rushJumbFrames.isEmpty()) {
            qDebug() << "Failed to load player_enhanced_k_rush_jumb_bright.gif frames";
        }
    }
}

Game::Game(QWidget *parent)
    : QGraphicsView(parent),
      upPressed(false), downPressed(false), leftPressed(false), rightPressed(false),
      canTeleport(true), isTeleporting(false),
      isMainMenuActive(true), isGameMenuActive(false),
      mainMenuStartItem(nullptr), mainMenuAboutItem(nullptr), mainMenuExitItem(nullptr),
      gameMenuButton(nullptr),
      gameMenuContinueItem(nullptr), gameMenuAboutItem(nullptr), gameMenuExitItem(nullptr)
      , introShownForSchoolMap(false)
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    resize(800, 600);
    setMinimumSize(400, 300);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWindowTitle("Qt-Gaming");

    // 预加载所有帧缓存（启动时一次解码，运行时零开销）
    preloadProjectileFrames();
    loadBombFrames();
    loadFireBgFrames();
    loadDaolangFrames();
    loadFireHitFrames();
    loadRushJumbFrames();
    preloadMonsterFrames();

    // 显示主菜单（不加载地图）
    setupEmptyScene();
    showMainMenu();
}

void Game::setupEmptyScene()
{
    // 清空场景中所有项
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        delete item;
    }
    // 设置一个深色背景矩形，覆盖整个视图
    QGraphicsRectItem *bg = new QGraphicsRectItem(0, 0, 800, 600);
    bg->setBrush(QBrush(QColor(30, 30, 40)));
    bg->setPen(Qt::NoPen);
    scene->addItem(bg);
    scene->setSceneRect(0, 0, 800, 600);
    setSceneRect(scene->sceneRect());
}

void Game::showMainMenu()
{
    isMainMenuActive = true;

    QFont titleFont("Arial", 32, QFont::Bold);
    QFont itemFont("Arial", 20);

    QGraphicsSimpleTextItem *title = new QGraphicsSimpleTextItem("Qt-Gaming");
    title->setFont(titleFont);
    title->setBrush(Qt::white);
    title->setPos(400 - title->boundingRect().width()/2, 150);
    scene->addItem(title);

    mainMenuStartItem = new QGraphicsSimpleTextItem("开始游戏");
    mainMenuStartItem->setFont(itemFont);
    mainMenuStartItem->setBrush(Qt::white);
    mainMenuStartItem->setPos(400 - mainMenuStartItem->boundingRect().width()/2, 300);
    scene->addItem(mainMenuStartItem);

    mainMenuAboutItem = new QGraphicsSimpleTextItem("关于");
    mainMenuAboutItem->setFont(itemFont);
    mainMenuAboutItem->setBrush(Qt::white);
    mainMenuAboutItem->setPos(400 - mainMenuAboutItem->boundingRect().width()/2, 370);
    scene->addItem(mainMenuAboutItem);

    mainMenuExitItem = new QGraphicsSimpleTextItem("退出");
    mainMenuExitItem->setFont(itemFont);
    mainMenuExitItem->setBrush(Qt::white);
    mainMenuExitItem->setPos(400 - mainMenuExitItem->boundingRect().width()/2, 440);
    scene->addItem(mainMenuExitItem);
}

void Game::hideMainMenu()
{
    if (mainMenuStartItem) { delete mainMenuStartItem; mainMenuStartItem = nullptr; }
    if (mainMenuAboutItem) { delete mainMenuAboutItem; mainMenuAboutItem = nullptr; }
    if (mainMenuExitItem)  { delete mainMenuExitItem;  mainMenuExitItem  = nullptr; }
    isMainMenuActive = false;
}

void Game::startLoadingPulse()
{
    if (!mainMenuStartItem) return;
    mainMenuStartItem->setBrush(QBrush(QColor(255, 50, 50)));
    if (loadingPulseTimer) { loadingPulseTimer->stop(); delete loadingPulseTimer; }
    loadingPulseTimer = new QTimer(this);
    int *tick = new int(0);
    connect(loadingPulseTimer, &QTimer::timeout, [this, tick]() {
        (*tick)++;
        if (!mainMenuStartItem) return;
        int bright = 50 + (int)(80 * (1.0 + qSin((*tick) * 0.3)) / 2.0);
        mainMenuStartItem->setBrush(QBrush(QColor(255, bright, bright)));
    });
    loadingPulseTimer->start(80);
}

void Game::startGame()
{
    // 按钮变红+刷新一帧
    if (mainMenuStartItem) mainMenuStartItem->setBrush(QBrush(QColor(255, 50, 50)));
    QApplication::processEvents();
    hideMainMenu();
    loadMap(":/maps/new_school_map.tmj", true);
}

void Game::quitGame()
{
    qApp->quit();
}

void Game::createGameMenuButton()
{
    if (!scene) return;
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    // 半透明暗底
    painter.setBrush(QBrush(QColor(20, 20, 30, 180)));
    painter.setPen(QPen(QColor(100, 120, 180, 200), 2));
    painter.drawRoundedRect(2, 2, 36, 36, 10, 10);
    // 三横线
    painter.setPen(QPen(QColor(200, 210, 240), 3));
    painter.drawLine(10, 14, 30, 14);
    painter.drawLine(10, 20, 30, 20);
    painter.drawLine(10, 26, 30, 26);
    painter.end();

    gameMenuButton = new QGraphicsPixmapItem(pixmap);
    gameMenuButton->setZValue(2000);
    gameMenuButton->setAcceptHoverEvents(true);
    scene->addItem(gameMenuButton);
    updateGameMenuButtonPosition();
}

void Game::updateGameMenuButtonPosition()
{
    if (!gameMenuButton) return;
    // 放在小地图下方，避免遮挡
    QPointF scenePos = mapToScene(viewport()->width() - 50, 70);
    gameMenuButton->setPos(scenePos);
}

void Game::createSkillBar()
{
    if (!scene) return;
    // 清理旧技能栏
    for (auto *item : skillBarItems) { scene->removeItem(item); delete item; }
    skillBarItems.clear();

    struct { QString key; QColor color; } skills[] = {
        {"I", QColor(255, 100, 60)},    // 宿命火球 - 橙红
        {"J", QColor(255, 60, 60)},      // 八面炼狱 - 红
        {"K", QColor(100, 200, 255)},    // 瞬影浪斩 - 蓝
        {"N", QColor(80, 160, 255)},     // 月牙八刃 - 蓝
        {"H", QColor(255, 200, 50)},     // 熔岩弹 - 金
        {"L", QColor(100, 220, 180)},    // 玄武盾 - 青绿
        {"O", QColor(200, 140, 255)},    // 加速 - 紫
    };
    const int n = 7;
    const int r = 18;  // 圆半径

    for (int i = 0; i < n; i++) {
        // 背景圆
        auto *circle = new QGraphicsEllipseItem(-r, -r, r*2, r*2);
        circle->setBrush(QBrush(QColor(20, 20, 30, 200)));
        circle->setPen(QPen(skills[i].color, 2));
        circle->setZValue(2000);
        scene->addItem(circle);
        skillBarItems.append(circle);

        // 字母
        auto *text = new QGraphicsSimpleTextItem(skills[i].key);
        text->setBrush(QBrush(skills[i].color));
        QFont f; f.setPointSize(12); f.setBold(true);
        text->setFont(f);
        text->setZValue(2001);
        text->setPos(-text->boundingRect().width()/2, -text->boundingRect().height()/2);
        scene->addItem(text);
        skillBarItems.append(text);
    }
    updateSkillBarPosition();
}

void Game::updateSkillBarPosition()
{
    if (skillBarItems.isEmpty()) return;
    const int n = 7;
    const int spacing = 50;  // 圆心间距
    int vpW = viewport()->width();
    int vpH = viewport()->height();

    for (int i = 0; i < n; i++) {
        // 底部居中排列
        qreal cx = vpW/2.0 - (n-1)*spacing/2.0 + i*spacing;
        qreal cy = vpH - 35;
        QPointF scenePos = mapToScene((int)cx, (int)cy);
        // 圆 (index 2*i) + 文字 (index 2*i+1)
        skillBarItems[2*i]->setPos(scenePos);
        skillBarItems[2*i+1]->setPos(scenePos);
        bool vis = skillBarVisible && !debugMapView;
        skillBarItems[2*i]->setVisible(vis);
        skillBarItems[2*i+1]->setVisible(vis);
    }
}

void Game::showGameMenu()
{
    if (isGameMenuActive) return;
    isGameMenuActive = true;
    // 暂停游戏循环
    if (gameTimer) gameTimer->stop();

    // ========== 关键：清除所有移动按键状态，避免菜单期间积累 ==========
    upPressed = downPressed = leftPressed = rightPressed = false;

    // 获取当前视图中心点（场景坐标）
    QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
    qreal centerX = viewRect.center().x();
    qreal centerY = viewRect.center().y();

    QFont itemFont("Arial", 18);

    gameMenuContinueItem = new QGraphicsSimpleTextItem("继续游戏");
    gameMenuContinueItem->setFont(itemFont);
    gameMenuContinueItem->setBrush(Qt::white);
    gameMenuContinueItem->setPos(centerX - gameMenuContinueItem->boundingRect().width()/2, centerY - 60);
    gameMenuContinueItem->setZValue(2001);
    scene->addItem(gameMenuContinueItem);

    gameMenuAboutItem = new QGraphicsSimpleTextItem("关于");
    gameMenuAboutItem->setFont(itemFont);
    gameMenuAboutItem->setBrush(Qt::white);
    gameMenuAboutItem->setPos(centerX - gameMenuAboutItem->boundingRect().width()/2, centerY);
    gameMenuAboutItem->setZValue(2001);
    scene->addItem(gameMenuAboutItem);

    gameMenuExitItem = new QGraphicsSimpleTextItem("退出游戏");
    gameMenuExitItem->setFont(itemFont);
    gameMenuExitItem->setBrush(Qt::white);
    gameMenuExitItem->setPos(centerX - gameMenuExitItem->boundingRect().width()/2, centerY + 60);
    gameMenuExitItem->setZValue(2001);
    scene->addItem(gameMenuExitItem);
}

void Game::hideGameMenu()
{
    if (!isGameMenuActive) return;
    isGameMenuActive = false;

    delete gameMenuContinueItem; gameMenuContinueItem = nullptr;
    delete gameMenuAboutItem;    gameMenuAboutItem = nullptr;
    delete gameMenuExitItem;     gameMenuExitItem = nullptr;

    // 再次清空，防止残留（可选）
    upPressed = downPressed = leftPressed = rightPressed = false;

    // 恢复游戏循环
    if (gameTimer) gameTimer->start(16);
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
    if (minimapPosText) { delete minimapPosText; minimapPosText = nullptr; }
    for (auto &d : diamonds) { delete d.item; }
    diamonds.clear();
    for (auto *s : portalSprites) { delete s; }
    portalSprites.clear();
    fogOverlay = nullptr;
    treeOverlays.clear();
    gatewayTrees.clear();
    secretTrees.clear();
    gatewayRoof = nullptr;
    secretRoof = nullptr;
    blackCurtain = nullptr;
    portalTransitionActive = false;
    portalTransitionPhase = 0;
    if (buffIndicator) { delete buffIndicator; buffIndicator = nullptr; }

    delete tileMap;
    delete player;
}

// 地图介绍配置（地图文件路径 -> 结构体）
struct IntroConfig {
    QString title;
    QString description;
    QString portraitPath;
};

static QMap<QString, IntroConfig> buildIntroMap() {
    QMap<QString, IntroConfig> map;
    map[":/maps/new_school_map.tmj"] = {
        "校史馆",
        "欢迎来到校史馆！\n\n这里是了解学校历史的起点。\n\n请在此接受初始训练，然后通过传送门前往其他地图探索。",
        ":/images/main_menu_bg.jpg"  // 请准备相应立绘资源，若无则用默认
    };
    map[":/maps/chamber1.tmj"] = {
        "密室",
        "你来到了密室。\n\n这里隐藏着学校深层的秘密……",
        ":/images/main_menu_bg.jpg"
    };
    map[":/maps/lianda.tmj"] = {
        "西南联大",
        "西南联大，战火中的教育奇迹。\n\n无数先辈在此求学，为中华崛起而奋斗。",
        ":/images/main_menu_bg.jpg"
    };
    map[":/maps/Weiming_lake.tmj"] = {
        "未名湖",
        "未名湖，燕园明珠。\n\n湖光塔影，百年学府的象征。",
        ":/images/main_menu_bg.jpg"
    };
    return map;
}

void Game::showIntroDialog(const QString &mapKey)
{
    static QMap<QString, IntroConfig> introMap = buildIntroMap();
    if (!introMap.contains(mapKey)) return;

    const IntroConfig &cfg = introMap[mapKey];

    // 暂停游戏
    if (gameTimer) gameTimer->stop();

    // ========== 关键：清除所有移动按键状态，避免对话框关闭后继续移动 ==========
    upPressed = downPressed = leftPressed = rightPressed = false;

    // 创建模态对话框
    QDialog dialog(this);
    dialog.setWindowTitle(cfg.title);
    dialog.setModal(true);
    dialog.setFixedSize(600, 450);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // 立绘（如果有）
    QLabel *portraitLabel = new QLabel;
    QPixmap portrait(cfg.portraitPath);
    if (!portrait.isNull()) {
        portrait = portrait.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        portraitLabel->setPixmap(portrait);
        portraitLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(portraitLabel);
    }

    // 介绍文本
    QTextBrowser *textBrowser = new QTextBrowser;
    textBrowser->setPlainText(cfg.description);
    textBrowser->setMinimumHeight(150);
    layout->addWidget(textBrowser);

    // 跳过按钮
    QPushButton *skipBtn = new QPushButton("开始探索");
    layout->addWidget(skipBtn, 0, Qt::AlignCenter);

    QObject::connect(skipBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    // 显示对话框（阻塞）
    dialog.exec();

    // 再次清空，防止残留（可选，但推荐）
    upPressed = downPressed = leftPressed = rightPressed = false;

    // 恢复游戏
    if (gameTimer) gameTimer->start(16);
}

void Game::checkAndShowIntro()
{
    if (!player || !tileMap || !scene) return;
    // 如果已经有对话框显示或游戏暂停，不再检测
    if (gamePaused || isGameMenuActive || isMainMenuActive) return;

    QString currentMap = currentMapPath;

    // 特殊处理：校史馆介绍（在出生点立刻触发，无需等待走出传送门）
    if (currentMap.contains("new_school_map") && !introShownForSchoolMap) {
        // 避免在传送过程中或玩家位置未设置时误触发
        if (player->pos() != QPointF(0,0) && !isTeleporting) {
            introShownForSchoolMap = true;
            showIntroDialog(currentMap);
        }
        return;
    }

    // 其他地图：只有当该地图未被介绍过，并且是从校史馆传送过来，且玩家已离开传送门范围
    if (!introShownForTargetMaps.contains(currentMap)) {
        // 判断是否从校史馆传送过来的：我们可以在跨地图传送时记录一个标志
        // 简单方案：检查当前地图是目标地图，且之前没有介绍，且玩家不在任何传送门附近（即已离开）
        // 为了避免重复判断，我们在 performTeleport 中设置一个标志 waitingForIntro
        // 下面实现该标志
        if (waitingForIntro && waitingIntroMap == currentMap) {
            if (!isNearPortal()) {
                waitingForIntro = false;
                introShownForTargetMaps.insert(currentMap);
                showIntroDialog(currentMap);
            }
        }
    }
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
    // 清理菜单按钮（如果存在）
    if (gameMenuButton) {
        gameMenuButton = nullptr;
    }
    for (auto *item : skillBarItems) { delete item; }
    skillBarItems.clear();
    for (auto &dz : dangerZones) { delete dz.circle; delete dz.cross1; delete dz.cross2; }
    dangerZones.clear();
    dangerSpawnTimer = 0;

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
    if (minimapPosText) { delete minimapPosText; minimapPosText = nullptr; }
    for (auto &d : diamonds) { delete d.item; }
    diamonds.clear();
    attackBuffTimer = 0;
    for (auto *s : portalSprites) { delete s; }
    portalSprites.clear();
    fogOverlay = nullptr;
    treeOverlays.clear();
    gatewayTrees.clear();
    secretTrees.clear();
    gatewayRoof = nullptr;
    secretRoof = nullptr;
    blackCurtain = nullptr;
    portalTransitionActive = false;
    portalTransitionPhase = 0;
    if (buffIndicator) { delete buffIndicator; buffIndicator = nullptr; }

    // 清空可攻击对象列表
    hittableItems.clear();

    // 清空宝箱和门列表（旧列表中的 Tile 对象将在场景清理时自动删除）
    chests.clear();
    doors.clear();
    interactionSpots.clear();
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
                    if (debugMapView) continue;  // 调试模式不生成敌人
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

                // ========== 处理 water 图层（lianda 和 Weiming_lake 边缘判定）==========
                if (layerName == "water") {
                    QVector<Tile*> waterTiles;
                    int waterCount = 0;
                    bool isLianda = mapFilePath.contains("lianda");
                    bool isWeimingLake = mapFilePath.contains("Weiming_lake");
                    bool useEdgeDetection = (isLianda || isWeimingLake);

                    // 确定图片前缀
                    QString prefix;
                    if (isLianda) {
                        prefix = ":/images/water";
                    } else if (isWeimingLake) {
                        prefix = ":/images/water_d";
                    }

                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;

                            QString waterPath;

                            if (useEdgeDetection) {
                                // 获取八个方向的水状态
                                bool hasUp = (y > 0 && ((dataArr[(y-1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                                bool hasDown = (y + 1 < mapHeight && ((dataArr[(y+1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                                bool hasLeft = (x > 0 && ((dataArr[y * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                                bool hasRight = (x + 1 < mapWidth && ((dataArr[y * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));

                                bool hasUpLeft = (y > 0 && x > 0 && ((dataArr[(y-1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                                bool hasUpRight = (y > 0 && x + 1 < mapWidth && ((dataArr[(y-1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                                bool hasDownLeft = (y + 1 < mapHeight && x > 0 && ((dataArr[(y+1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                                bool hasDownRight = (y + 1 < mapHeight && x + 1 < mapWidth && ((dataArr[(y+1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));

                                int directionCount = (hasUp ? 1 : 0) + (hasDown ? 1 : 0) + (hasLeft ? 1 : 0) + (hasRight ? 1 : 0);

                                // 规则1：只有一个方向不是水
                                if (directionCount == 3) {
                                    if (!hasUp) waterPath = prefix + "_north.png";
                                    else if (!hasDown) waterPath = prefix + "_south.png";
                                    else if (!hasLeft) waterPath = prefix + "_west.png";
                                    else if (!hasRight) waterPath = prefix + "_east.png";
                                }
                                // 规则2：两个相邻方向不是水
                                else if (directionCount == 2) {
                                    bool upLeftEmpty = !hasUp && !hasLeft;
                                    bool downLeftEmpty = !hasDown && !hasLeft;
                                    bool downRightEmpty = !hasDown && !hasRight;
                                    bool upRightEmpty = !hasUp && !hasRight;

                                    if (upLeftEmpty) waterPath = prefix + "_northwest_1.png";
                                    else if (downLeftEmpty) waterPath = prefix + "_southwest_1.png";
                                    else if (downRightEmpty) waterPath = prefix + "_southeast_1.png";
                                    else if (upRightEmpty) waterPath = prefix + "_northeast_1.png";
                                    else {
                                        // 非相邻方向（如上下或左右），走默认随机
                                        int r = QRandomGenerator::global()->bounded(100);
                                        if (r < 60) waterPath = prefix + "_1.png";
                                        else if (r < 95) waterPath = prefix + "_2.png";
                                        else waterPath = prefix + "_3.png";
                                    }
                                }
                                // 规则3：四个方向都是水，检查角落
                                else if (directionCount == 4) {
                                    if (!hasUpLeft) waterPath = prefix + "_northwest_2.png";
                                    else if (!hasDownLeft) waterPath = prefix + "_southwest_2.png";
                                    else if (!hasUpRight) waterPath = prefix + "_northeast_2.png";
                                    else if (!hasDownRight) waterPath = prefix + "_southeast_2.png";
                                    else {
                                        // 完全包围，走默认随机
                                        int r = QRandomGenerator::global()->bounded(100);
                                        if (r < 60) waterPath = prefix + "_1.png";
                                        else if (r < 95) waterPath = prefix + "_2.png";
                                        else waterPath = prefix + "_3.png";
                                    }
                                }
                                // 规则4：其他情况（孤立水块等）
                                else {
                                    int r = QRandomGenerator::global()->bounded(100);
                                    if (r < 60) waterPath = prefix + "_1.png";
                                    else if (r < 95) waterPath = prefix + "_2.png";
                                    else waterPath = prefix + "_3.png";
                                }
                            } else {
                                // 其他地图：简单随机
                                int r = QRandomGenerator::global()->bounded(100);
                                if (r < 30) waterPath = ":/images/water_1.png";
                                else waterPath = ":/images/water_2.png";
                            }

                            Tile *waterTile = new Tile(waterPath, x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            waterTile->setZValue(1);
                            scene->addItem(waterTile);
                            waterTiles.append(waterTile);
                            waterCount++;
                        }
                    }
                    if (!waterTiles.isEmpty()) {
                        tileMap->addWaterTiles(waterTiles);
                    }
                    qDebug() << "[ManualDraw] Water layer created" << waterCount << "water tiles";
                    continue;
                }

                // ========== 处理 fireland 图层（持续掉血 + 方向检测）==========
                if (layerName == "fireland") {
                    int fireCount = 0;
                    for (int y = 0; y < mapHeight; ++y) {
                        for (int x = 0; x < mapWidth; ++x) {
                            int rawGid = dataArr[y * mapWidth + x].toInt();
                            int cleanGid = rawGid & 0x1FFFFFFF;
                            if (cleanGid == 0) continue;

                            QRectF fireRect(x * tileWidth, y * tileHeight, tileWidth, tileHeight);
                            fireRects.append(fireRect);           // 保留原有列表（可选，也可删除）
                            tileMap->addFireRect(fireRect);       // 添加到空间索引

                            // 八方向邻居检测
                            bool hasUp = (y > 0 && ((dataArr[(y-1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDown = (y + 1 < mapHeight && ((dataArr[(y+1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                            bool hasLeft = (x > 0 && ((dataArr[y * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasRight = (x + 1 < mapWidth && ((dataArr[y * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasUpLeft = (y > 0 && x > 0 && ((dataArr[(y-1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasUpRight = (y > 0 && x + 1 < mapWidth && ((dataArr[(y-1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDownLeft = (y + 1 < mapHeight && x > 0 && ((dataArr[(y+1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDownRight = (y + 1 < mapHeight && x + 1 < mapWidth && ((dataArr[(y+1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            int dirCount = (hasUp?1:0)+(hasDown?1:0)+(hasLeft?1:0)+(hasRight?1:0);

                            QString firePath;
                            if (dirCount == 3) {
                                if (!hasUp) firePath = ":/images/fireland_north.png";
                                else if (!hasDown) firePath = ":/images/fireland_south.png";
                                else if (!hasLeft) firePath = ":/images/fireland_west.png";
                                else if (!hasRight) firePath = ":/images/fireland_east.png";
                            } else if (dirCount == 2) {
                                bool ulE = !hasUp && !hasLeft, dlE = !hasDown && !hasLeft;
                                bool drE = !hasDown && !hasRight, urE = !hasUp && !hasRight;
                                if (ulE) firePath = ":/images/fireland_northwest_1.png";
                                else if (dlE) firePath = ":/images/fireland_southwest_1.png";
                                else if (drE) firePath = ":/images/fireland_southeast_1.png";
                                else if (urE) firePath = ":/images/fireland_northeast_1.png";
                                else { int r = QRandomGenerator::global()->bounded(3); firePath = QString(":/images/fireland_%1.png").arg(r+1); }
                            } else if (dirCount == 4) {
                                if (!hasUpLeft) firePath = ":/images/fireland_northwest_2.png";
                                else if (!hasDownLeft) firePath = ":/images/fireland_southwest_2.png";
                                else if (!hasUpRight) firePath = ":/images/fireland_northeast_2.png";
                                else if (!hasDownRight) firePath = ":/images/fireland_southeast_2.png";
                                else { int r = QRandomGenerator::global()->bounded(3); firePath = QString(":/images/fireland_%1.png").arg(r+1); }
                            } else {
                                int r = QRandomGenerator::global()->bounded(3);
                                firePath = QString(":/images/fireland_%1.png").arg(r+1);
                            }

                            Tile *fireTile = new Tile(firePath, x * tileWidth, y * tileHeight, QSize(tileWidth, tileHeight));
                            fireTile->setZValue(-4);
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
                        else if (layerName == "floor_outside") {
                            finalPath = ":/images/floor_outside.png";
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            tileMap->addWallTile(tile);  // 添加碰撞，钻石不会生成在碰撞区域
                            tileCount++;
                            continue;
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
                        // 桌子图层：方向自动检测 + 碰撞
                        else if (layerName == "tabel") {
                            bool hasUp = (y > 0 && ((dataArr[(y-1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDown = (y + 1 < mapHeight && ((dataArr[(y+1) * mapWidth + x].toInt() & 0x1FFFFFFF) != 0));
                            bool hasLeft = (x > 0 && ((dataArr[y * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasRight = (x + 1 < mapWidth && ((dataArr[y * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasUpLeft = (y > 0 && x > 0 && ((dataArr[(y-1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasUpRight = (y > 0 && x + 1 < mapWidth && ((dataArr[(y-1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDownLeft = (y + 1 < mapHeight && x > 0 && ((dataArr[(y+1) * mapWidth + (x-1)].toInt() & 0x1FFFFFFF) != 0));
                            bool hasDownRight = (y + 1 < mapHeight && x + 1 < mapWidth && ((dataArr[(y+1) * mapWidth + (x+1)].toInt() & 0x1FFFFFFF) != 0));
                            int dirCount = (hasUp?1:0)+(hasDown?1:0)+(hasLeft?1:0)+(hasRight?1:0);
                            if (dirCount == 3) {
                                if (!hasUp) finalPath = ":/images/tabel_north.png";
                                else if (!hasDown) finalPath = ":/images/tabel_south.png";
                                else if (!hasLeft) finalPath = ":/images/tabel_west.png";
                                else if (!hasRight) finalPath = ":/images/tabel_east.png";
                            } else if (dirCount == 2) {
                                bool ulE = !hasUp && !hasLeft, dlE = !hasDown && !hasLeft;
                                bool drE = !hasDown && !hasRight, urE = !hasUp && !hasRight;
                                if (ulE) finalPath = ":/images/tabel_northwest.png";
                                else if (dlE) finalPath = ":/images/tabel_southwest.png";
                                else if (drE) finalPath = ":/images/tabel_southeast.png";
                                else if (urE) finalPath = ":/images/tabel_northeast.png";
                                else { int r = QRandomGenerator::global()->bounded(100); finalPath = QString(":/images/tabel_%1.png").arg(r<70?1:r<85?2:3); }
                            } else if (dirCount == 4) {
                                if (!hasUpLeft) finalPath = ":/images/tabel_northwest.png";
                                else if (!hasDownLeft) finalPath = ":/images/tabel_southwest.png";
                                else if (!hasUpRight) finalPath = ":/images/tabel_northeast.png";
                                else if (!hasDownRight) finalPath = ":/images/tabel_southeast.png";
                                else { int r = QRandomGenerator::global()->bounded(100); finalPath = QString(":/images/tabel_%1.png").arg(r<70?1:r<85?2:3); }
                            } else {
                                int r = QRandomGenerator::global()->bounded(100);
                                finalPath = QString(":/images/tabel_%1.png").arg(r<70?1:r<85?2:3);
                            }
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            tileMap->addWallTile(tile);
                            tileCount++;
                            continue;
                        }
                        // 装饰物图层：随机 decoration_1/2/3
                        else if (layerName == "decorations") {
                            int r = QRandomGenerator::global()->bounded(100);
                            if (r < 20) finalPath = ":/images/decoration_1.png";
                            else if (r < 60) finalPath = ":/images/decoration_2.png";
                            else finalPath = ":/images/decoration_3.png";
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
                        else if (layerName == "interaction_1") {
                            finalPath = ":/images/interaction_1.png";
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            tileMap->addWallTile(tile);
                            interactionSpots.append({QPoint(x, y), 1});
                            tileCount++;
                            continue;
                        }
                        else if (layerName == "interaction_2") {
                            finalPath = ":/images/interaction_2.png";
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            tileMap->addWallTile(tile);
                            interactionSpots.append({QPoint(x, y), 2});
                            tileCount++;
                            continue;
                        }
                        else if (layerName == "interaction_wall") {
                            finalPath = ":/images/interaction_wall.png";
                            Tile *tile = new Tile(finalPath, x * tileWidth, y * tileHeight, fixedSize);
                            scene->addItem(tile);
                            tileMap->addWallTile(tile);
                            interactionSpots.append({QPoint(x, y), 3});
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

                        // ========== 渲染层级（Z值）==========
                        // 调试模式：全部置顶；正常模式：草(-4)→水(-3)→路(-2)→墙(-1)→装饰(0)
                        if (debugMapView) {
                            tile->setZValue(10000);
                        } else if (layerName == "grass" || layerName == "grassland") {
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
            if (!debugMapView) {
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
            } // !debugMapView — 调试模式不创建传送门精灵
        } else {
            qDebug() << "[ManualDraw] Failed to parse JSON for manual drawing:" << mapFilePath;
        }
        file.close();
    } else {
        qDebug() << "[ManualDraw] Cannot open map file for manual drawing:" << mapFilePath;
    }

    // ========== 主地图区域标注叠加层（仅 new_school_map，Z=-0.5 夹在地形和物体之间）==========
    if (!debugMapView && mapFilePath.contains("new_school_map")) {
        struct Overlay {
            QString path;
            int tileX, tileY, tileW, tileH; // 左上角 tile 坐标 + 覆盖范围（tile 数）
        };
        const Overlay overlays[] = {
            {":/images/maze.png",               120, 25, 32, 22},
            {":/images/gateway_better.png",       159, 35, 42, 12},
            {":/images/garden.png",             204, 10, 52, 42},
            {":/images/school_secret_place.png", 241, 58, 50, 20},
        };
        int ts = tileMap->getTileWidth();
        for (const auto &ov : overlays) {
            QPixmap pm(ov.path);
            if (pm.isNull()) continue;
            int pxW = ov.tileW * ts;
            int pxH = ov.tileH * ts;
            auto *item = new QGraphicsPixmapItem();
            item->setPixmap(pm);
            item->setTransformationMode(Qt::SmoothTransformation);
            qreal sx = static_cast<qreal>(pxW) / pm.width();
            qreal sy = static_cast<qreal>(pxH) / pm.height();
            item->setTransform(QTransform::fromScale(sx, sy));
            item->setPos(ov.tileX * ts, ov.tileY * ts);
            item->setZValue(-0.5);  // 夹在地形(-2~-1)和物体(0+)之间
            scene->addItem(item);
        }
        qDebug() << "[Overlay] Region overlays placed for new_school_map";

        // ===== 树：围绕区域1(garden 204,10~255,51)外延1格放一圈，随机间隔2~5格，随机大小5~13奇数 =====
        {
            struct Pt { int x, y; };
            QVector<Pt> perimeter;
            // 上边 y=9, x=203..256
            for (int x = 203; x <= 256; x++) perimeter.append({x, 9});
            // 右边 x=256, y=10..51
            for (int y = 10; y <= 51; y++) perimeter.append({256, y});
            // 下边 y=52, x=255..203
            for (int x = 255; x >= 203; x--) perimeter.append({x, 52});
            // 左边 x=203, y=51..10
            for (int y = 51; y >= 10; y--) perimeter.append({203, y});

            int spacing = QRandomGenerator::global()->bounded(2, 6); // 2~5
            int stepCount = 0;
            struct Tree { int cx, cy, sz; };
            QVector<Tree> trees;

            for (const auto &pt : perimeter) {
                if (stepCount >= spacing) {
                    stepCount = 0;
                    spacing = QRandomGenerator::global()->bounded(2, 6);
                    int sz = 5 + QRandomGenerator::global()->bounded(5) * 2; // 5,7,9,11,13
                    if (pt.x == 203 && pt.y == 40) sz = 7;       // 强制7×7
                    trees.append({pt.x, pt.y, sz});
                }
                stepCount++;
            }
            // 强制 (244,54) 13×13
            trees.append({244, 54, 13});

            for (const auto &t : trees) {
                QPixmap pm(":/images/tree.png");
                if (pm.isNull()) continue;
                int pxSz = t.sz * ts;
                int pxX = (t.cx - t.sz / 2) * ts;
                int pxY = (t.cy - t.sz / 2) * ts;
                auto *item = new QGraphicsPixmapItem();
                item->setPixmap(pm);
                item->setTransformationMode(Qt::SmoothTransformation);
                qreal s = static_cast<qreal>(pxSz) / pm.width();
                item->setTransform(QTransform::fromScale(s, s));
                item->setPos(pxX, pxY);
                item->setZValue(502);
                item->setOpacity(1.0);  // 默认完全可见
                scene->addItem(item);
                treeOverlays.append(item);
            }
            qDebug() << "[Tree] Placed" << trees.size() << "trees around garden perimeter";
        }

        // ===== winter_tree：围绕区域2(gateway)左半边边缘（仅上下左，无中间分界）=====
        {
            struct Pt { int x, y; };
            QVector<Pt> wperim;
            for (int x = 158; x <= 179; x++) wperim.append({x, 34});  // 上
            for (int x = 179; x >= 158; x--) wperim.append({x, 47});   // 下
            for (int y = 46; y >= 35; y--)  wperim.append({158, y});   // 左

            int wspacing = QRandomGenerator::global()->bounded(2, 6);
            int wstep = 0;
            for (const auto &pt : wperim) {
                if (wstep >= wspacing) {
                    wstep = 0;
                    wspacing = QRandomGenerator::global()->bounded(2, 6);
                    int sz = 5 + QRandomGenerator::global()->bounded(5) * 2;
                    QPixmap pm(":/images/winter_tree.png");
                    if (!pm.isNull()) {
                        auto *item = new QGraphicsPixmapItem();
                        item->setPixmap(pm);
                        item->setTransformationMode(Qt::SmoothTransformation);
                        qreal s = static_cast<qreal>(sz * ts) / pm.width();
                        item->setTransform(QTransform::fromScale(s, s));
                        item->setPos((pt.x - sz/2) * ts, (pt.y - sz/2) * ts);
                        item->setZValue(502);
                        item->setOpacity(0.75);
                        scene->addItem(item);
                        gatewayTrees.append(item);
                    }
                }
                wstep++;
            }
        // 强制 (153,41) 15×15 winter_tree
        { QPixmap pm(":/images/winter_tree.png"); if (!pm.isNull()) { auto *item = new QGraphicsPixmapItem(); item->setPixmap(pm); item->setTransformationMode(Qt::SmoothTransformation); qreal s = static_cast<qreal>(15*32)/pm.width(); item->setTransform(QTransform::fromScale(s,s)); item->setPos((153-7)*32, (41-7)*32); item->setZValue(502); scene->addItem(item); gatewayTrees.append(item); } }
        }

        // ===== tree：区域2(gateway)右半边边缘（仅上下右，无中间分界）=====
        {
            struct Pt { int x, y; };
            QVector<Pt> rperim;
            for (int x = 180; x <= 201; x++) rperim.append({x, 34});  // 上
            for (int y = 35; y <= 46; y++)  rperim.append({201, y});   // 右
            for (int x = 200; x >= 180; x--) rperim.append({x, 47});   // 下

            int tsp = QRandomGenerator::global()->bounded(2, 6);
            int stp = 0;
            for (const auto &pt : rperim) {
                if (stp >= tsp) {
                    stp = 0; tsp = QRandomGenerator::global()->bounded(2, 6);
                    int sz = 5 + QRandomGenerator::global()->bounded(5) * 2;
                    QPixmap pm(":/images/tree.png");
                    if (!pm.isNull()) {
                        auto *item = new QGraphicsPixmapItem();
                        item->setPixmap(pm);
                        item->setTransformationMode(Qt::SmoothTransformation);
                        qreal s = static_cast<qreal>(sz * ts) / pm.width();
                        item->setTransform(QTransform::fromScale(s, s));
                        item->setPos((pt.x - sz/2) * ts, (pt.y - sz/2) * ts);
                        item->setZValue(502);
                        item->setOpacity(0.75);
                        scene->addItem(item);
                        gatewayTrees.append(item);
                    }
                }
                stp++;
            }
            qDebug() << "[Tree] Placed around gateway right-half perimeter";
        }

        // ===== 雾：放大到区域3(maze)最长边1.2倍，置于区域3正中央 =====
        {
            // 区域3: (120,25)~(151,46), 宽32 高22, 最长边=32
            qreal fogTiles = 32.0 * 1.75;                   // 56 tiles
            // 保持在区域3(maze: 120~151, 25~46)正中央
            qreal fogPx = fogTiles * ts;
            qreal pxX = (120.0 + 151.0) / 2.0 * ts - fogPx / 2.0;
            qreal pxY = (25.0  + 46.0)  / 2.0 * ts - fogPx / 2.0;

            QPixmap pm(":/images/fog.png");
            if (!pm.isNull()) {
                auto *item = new QGraphicsPixmapItem();
                item->setPixmap(pm);
                item->setTransformationMode(Qt::SmoothTransformation);
                qreal s = fogPx / pm.width();
                item->setTransform(QTransform::fromScale(s, s));
                item->setPos(pxX, pxY);
                item->setZValue(500);
                item->setOpacity(0.0);   // 默认不可见，进入迷宫时才显示
                scene->addItem(item);
                fogOverlay = item;
                qDebug() << "[Fog] Placed fog" << fogTiles << "tiles over maze center";
            }
        }

        // ===== winter_tree：39×39 中心在 (155,40) =====
        {
            QPixmap pm(":/images/winter_tree.png");
            if (!pm.isNull()) {
                auto *item = new QGraphicsPixmapItem();
                int sz = 39 * 32;
                item->setPixmap(pm.scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                item->setTransformationMode(Qt::SmoothTransformation);
                item->setZValue(502);
                item->setOpacity(0.6);
                scene->addItem(item);
                gatewayTrees.append(item);  // 纳入统一管理
            }
        }

        // ===== autumn_tree：区域2中间1/3上下边缘 =====
        {
            QPixmap pm(":/images/autumn_tree.png");
            if (!pm.isNull()) {
                int spc = QRandomGenerator::global()->bounded(2, 6), stp = 0;
                for (int x = 173; x <= 186; x++) {
                    for (int y : {34, 47}) {
                        if (stp >= spc) {
                            stp = 0; spc = QRandomGenerator::global()->bounded(2, 6);
                            int sz = 5 + QRandomGenerator::global()->bounded(5) * 2;
                            auto *item = new QGraphicsPixmapItem();
                            item->setPixmap(pm);
                            item->setTransformationMode(Qt::SmoothTransformation);
                        qreal s = static_cast<qreal>(sz * ts) / pm.width();
                            item->setTransform(QTransform::fromScale(s,s));
                            item->setPos((x-sz/2)*ts, (y-sz/2)*ts);
                            item->setZValue(502);
                            item->setOpacity(1.0);
                            scene->addItem(item);
                            gatewayTrees.append(item);
                        }
                        stp++;
                    }
                }
            }
            qDebug() << "[AutumnTree] Placed on gateway middle-third top/bottom";
        }

        // ===== winter_tree：区域3(maze)周围，距离3 =====
        {
            struct Pt { int x, y; };
            QVector<Pt> mPerim;
            // maze: 120,25~151,46，距离3 → 外延3格
            for (int x = 117; x <= 154; x++) mPerim.append({x, 22});   // 上 (25-3)
            for (int y = 25; y <= 46; y++)  mPerim.append({154, y});    // 右 (151+3)
            for (int x = 153; x >= 117; x--) mPerim.append({x, 49});    // 下 (46+3)
            for (int y = 46; y >= 25; y--)  mPerim.append({117, y});    // 左 (120-3)
            int sp2 = QRandomGenerator::global()->bounded(2, 6), sc2 = 0;
            for (const auto &pt : mPerim) {
                if (sc2 >= sp2) {
                    sc2 = 0; sp2 = QRandomGenerator::global()->bounded(2, 6);
                    int sz = 5 + QRandomGenerator::global()->bounded(5) * 2;
                    QPixmap pm(":/images/winter_tree.png");
                    if (!pm.isNull()) {
                        auto *item = new QGraphicsPixmapItem();
                        item->setPixmap(pm);
                        item->setTransformationMode(Qt::SmoothTransformation);
                        qreal s = static_cast<qreal>(sz * ts) / pm.width();
                        item->setTransform(QTransform::fromScale(s,s));
                        item->setPos((pt.x-sz/2)*ts, (pt.y-sz/2)*ts);
                        item->setZValue(502);
                        item->setOpacity(0.75);
                        scene->addItem(item);
                    }
                }
                sc2++;
            }
            qDebug() << "[WinterTree] Placed around maze perimeter (distance 3)";
        }

        // ===== 展示图：lion(120,70) cat(150,70) 7(180,70)，长边25格 =====
        {
            struct { QString path; int cx, cy; } shows[] = {
                {":/images/show_lion.png", 150, 70},
                {":/images/show_cat.png",  180, 70},
                {":/images/show7.png",     210, 70},
            };
            for (auto &sh : shows) {
                QPixmap pm(sh.path);
                if (pm.isNull()) continue;
                qreal targetLen = 25.0 * 32;  // 800px
                qreal s = targetLen / qMax(pm.width(), pm.height());
                int w = (int)(pm.width() * s), h = (int)(pm.height() * s);
                auto *item = new QGraphicsPixmapItem(pm.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                item->setTransformationMode(Qt::SmoothTransformation);
                item->setPos(sh.cx * 32 - w/2, sh.cy * 32 - h/2);
                item->setZValue(503);
                scene->addItem(item);
            }
        }

        // ===== (155,43) 19×19 范围内 winter_tree 透明度 25% =====
        {
            qreal cx = 155.0 * ts, cy = 43.0 * ts;
            qreal half = 9.5 * ts;
            for (auto *t : gatewayTrees) {
                QPointF ic = t->sceneBoundingRect().center();
                if (ic.x() > cx-half && ic.x() < cx+half && ic.y() > cy-half && ic.y() < cy+half)
                    t->setOpacity(0.25);
            }
        }

        // ===== gateway_better_roof：覆盖 gateway_better (159,35) 42×12 =====
        {
            QPixmap pm(":/images/gateway_better_roof.png");
            if (!pm.isNull()) {
                auto *item = new QGraphicsPixmapItem();
                int pxW = 42 * ts, pxH = 12 * ts;
                item->setPixmap(pm);
                item->setTransformationMode(Qt::SmoothTransformation);
                item->setTransform(QTransform::fromScale((qreal)pxW/pm.width(), (qreal)pxH/pm.height()));
                item->setPos(159 * ts, 35 * ts);
                item->setZValue(501);
                item->setOpacity(1.0);
                scene->addItem(item);
                gatewayRoof = item;
            }
        }
        // ===== school_secret_place_roof：覆盖 secret_place (241,58) 50×20 =====
        {
            QPixmap pm(":/images/school_secret_place_roof.png");
            if (!pm.isNull()) {
                auto *item = new QGraphicsPixmapItem();
                int pxW = 50 * ts, pxH = 20 * ts;
                item->setPixmap(pm);
                item->setTransformationMode(Qt::SmoothTransformation);
                item->setTransform(QTransform::fromScale((qreal)pxW/pm.width(), (qreal)pxH/pm.height()));
                item->setPos(241 * ts, 58 * ts);
                item->setZValue(501);
                item->setOpacity(1.0);
                scene->addItem(item);
                secretRoof = item;
            }
        }
        // ===== tree：围绕区域4(secret 241,58~290,77)一圈 =====
        {
            struct Pt { int x, y; };
            QVector<Pt> sPerim;
            for (int x = 240; x <= 291; x++) sPerim.append({x, 57});   // 上
            for (int y = 58; y <= 77; y++)  sPerim.append({291, y});    // 右
            for (int x = 290; x >= 240; x--) sPerim.append({x, 78});    // 下
            for (int y = 77; y >= 58; y--)  sPerim.append({240, y});    // 左
            int sp = QRandomGenerator::global()->bounded(2, 6), sc = 0;
            for (const auto &pt : sPerim) {
                if (sc >= sp) {
                    sc = 0; sp = QRandomGenerator::global()->bounded(2, 6);
                    int sz = 5 + QRandomGenerator::global()->bounded(5) * 2;
                    QPixmap pm(":/images/tree.png");
                    if (!pm.isNull()) {
                        auto *item = new QGraphicsPixmapItem();
                        item->setPixmap(pm);
                        item->setTransformationMode(Qt::SmoothTransformation);
                        qreal s = static_cast<qreal>(sz * ts) / pm.width();
                        item->setTransform(QTransform::fromScale(s, s));
                        item->setPos((pt.x - sz/2) * ts, (pt.y - sz/2) * ts);
                        item->setZValue(502);
                        item->setOpacity(0.75);
                        scene->addItem(item);
                        secretTrees.append(item);
                    }
                }
                sc++;
            }
            qDebug() << "[Tree] Placed around secret perimeter";
        }
    }

    // ========== ending 地图额外叠加层 ==========
    if (!debugMapView && mapFilePath.contains("ending")) {
        int ts = tileMap->getTileWidth(); // 瓦片尺寸，一般为 32
        int mapW = tileMap->getMapWidth() * ts;
        int mapH = tileMap->getMapHeight() * ts;

        // 示例：添加一张全图背景（覆盖整个地图）
        QPixmap endingBg(":/images/ending.jpg");
        if (!endingBg.isNull()) {
            auto *bgItem = new QGraphicsPixmapItem();
            qreal sx = (qreal)mapW / endingBg.width();
            qreal sy = (qreal)mapH / endingBg.height();
            bgItem->setTransform(QTransform::fromScale(sx, sy));
            bgItem->setPixmap(endingBg);
            bgItem->setZValue(-0.5); // 放在地形之上，玩家之下
            bgItem->setTransformationMode(Qt::SmoothTransformation);
            scene->addItem(bgItem);
        }
    }

    // ---------- 4. 创建玩家 ----------
    if (!debugMapView) {
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
    // 如果是 ending 地图，则跳过巢穴生成（不生成任何怪物）
    if (!mapFilePath.contains("ending.tmj")) {
        QPointF spawnBase = playerStart;
        if (spawnBase.isNull()) spawnBase = QPointF(100, 100);
        int ts = tileMap->getTileWidth();
        int mapW = tileMap->getMapWidth() * ts;
        int mapH = tileMap->getMapHeight() * ts;

        // 安全放置函数：夹在可行走区域内，并尝试 3×3 搜索
        auto safeSpawn = [&](QPointF pos) -> QPointF {
            pos.setX(qBound(ts * 2.0, pos.x(), mapW - ts * 3.0));
            pos.setY(qBound(ts * 2.0, pos.y(), mapH - ts * 3.0));
            // 从中心向外螺旋搜索（3×3）
            for (int r = 0; r <= 3; r++) {
                for (int dy = -r; dy <= r; dy++) {
                    for (int dx = -r; dx <= r; dx++) {
                        if (qAbs(dx) != r && qAbs(dy) != r) continue; // 只检查当前环
                        QPointF test(pos.x() + dx * ts, pos.y() + dy * ts);
                        QRectF tr(test.x(), test.y(), 40, 40);
                        if (!tileMap->collidesWithWall(tr) && !tileMap->collidesWithWater(tr))
                            return test;
                    }
                }
            }
            return pos;  // 实在找不到就返回夹紧位置
        };

        QPointF offsets[] = {{512, 0}, {1600, 960}};
        for (auto &off : offsets) {
            QPointF sp = safeSpawn(spawnBase + off);
            spawners.append(new Spawner(tileMap, scene, sp, this));
        }
        qDebug() << "[loadMap] 2 spawners created (ending map skipped).";
    } else {
        qDebug() << "[loadMap] Ending map detected: no spawners created.";
    }

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

    } // if (!debugMapView) — 游戏对象创建结束

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
        if (debugMapView) {
            // 调试模式：镜头对准地图中心
            centerOn(scene->sceneRect().center());
        } else {
            centerOn(player);
        }
    }

    // 重置缩放
    zoomLevel = 1.0;
    applyZoom();

    // 创建 HUD
    if (!debugMapView) {
    createHud();
    qDebug() << "[loadMap] HUD created.";

    // 创建小地图
    QTimer::singleShot(100, this, [this]() { createMinimap(); });

    // 生成钻石
    spawnDiamonds();

    // 如果不是主菜单状态（即游戏进行中），创建右上角菜单按钮
    if (!isMainMenuActive) {
        createGameMenuButton();
        createSkillBar();
    }
    } // !debugMapView

    // ---------- 11. 重新启动游戏循环 ----------
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
        qDebug() << "[loadMap] Game timer created.";
    }
    gameTimer->start(16);
    qDebug() << "[loadMap] Game timer started (16ms interval).";

    // 停止加载脉冲
    if (loadingPulseTimer) { loadingPulseTimer->stop(); delete loadingPulseTimer; loadingPulseTimer = nullptr; }
    qDebug() << "[loadMap] Map loading completed.";
}

void Game::keyPressEvent(QKeyEvent *event)
{
    // 传送过渡期间禁用所有按键
    if (portalTransitionActive) return;

    // 菜单激活时，只处理 ESC 关闭游戏内菜单
    if (isMainMenuActive) {
        // 主菜单时不处理任何游戏键，也不退出（避免误操作）
        return;
    }
    if (isGameMenuActive) {
        if (event->key() == Qt::Key_Escape) {
            hideGameMenu();
        }
        return;
    }
    if (gamePaused) return;   
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
    case Qt::Key_I: if (!debugMapView && !isNearPortal()) skillMeteorBurst(); break;
    case Qt::Key_H: if (!debugMapView && !isNearPortal()) skillTriangleShot(); break;
    case Qt::Key_N: if (!debugMapView && !isNearPortal()) skillBlueBurst(); break;
    case Qt::Key_J:
        if (!debugMapView && !isNearPortal() && player) {
            if (player->getEnhanced()) {
                bool moving = upPressed || downPressed || leftPressed || rightPressed;
                if (moving && !jPlaying) {
                    // 跑动中按J：bright版，不中断移动，用skillNormalAttack逻辑
                    player->playCastAnimation(":/images/player_enhanced_pugong_bright.gif", 2, 77);
                    skillNormalAttack();  // 火圈+伤害
                } else if (!moving) {
                    // 静止：连击系统
                    if (jPressCount >= 4) break;
                    jPressCount++;
                    jPressTimer = J_PRESS_WINDOW;
                    jRushDir = (upPressed||downPressed||leftPressed||rightPressed) ? getCurrentDirectionVector() : lastMoveDir;
                    if (!jPlaying) {
                        jPlaying = true;
                        jPlayIndex = -1;
                        upPressed = downPressed = leftPressed = rightPressed = false;
                    }
                }
            } else {
                skillNormalAttack();
            }
        }
        break;
    case Qt::Key_K:
        if (debugMapView) break;
        // 闪现技能使用更严格的传送门检测（外扩 6 格）
        if (!isNearPortal(6)) {
            skillFlashBlade();
        }
        break;
    case Qt::Key_L: if (!debugMapView && !isNearPortal()) skillShieldActivate(); break;
    case Qt::Key_R:
        if (isNearInteraction()) {
            int type = getCurrentInteractionType();
            if (type > 0) showInteractionDialog(type);
        }
        break;
    case Qt::Key_O:
        if (debugMapView) break;
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
    case Qt::Key_F2:
        skillBarVisible = !skillBarVisible;
        updateSkillBarPosition();
        break;
    case Qt::Key_F1:
        debugMapView = !debugMapView;
        qDebug() << "Debug map view:" << (debugMapView ? "ON (terrain on top)" : "OFF");
        // Reload map to apply Z-value changes
        if (tileMap && !currentMapPath.isEmpty()) {
            loadMap(currentMapPath, false);
        }
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
    if (portalTransitionActive) return;
    if (isMainMenuActive || isGameMenuActive) return;
    if (gamePaused) return;   
    switch (event->key()) {
    case Qt::Key_W: upPressed = false; break;
    case Qt::Key_S: downPressed = false; break;
    case Qt::Key_A: leftPressed = false; break;
    case Qt::Key_D: rightPressed = false; break;
    case Qt::Key_L: if (!debugMapView) skillShieldDeactivate(); break; // ← L键释放：关闭玄武盾
    default: QGraphicsView::keyReleaseEvent(event);
    }
}

void Game::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());

    // ========== 主菜单点击 ==========
    if (isMainMenuActive) {
        if (mainMenuStartItem && mainMenuStartItem->contains(mainMenuStartItem->mapFromScene(scenePos))) {
            startLoadingPulse();
            startGame();
        } else if (mainMenuAboutItem && mainMenuAboutItem->contains(mainMenuAboutItem->mapFromScene(scenePos))) {
            QMessageBox::information(this, "关于", "Qt-Gaming 演示游戏\n版本 2.0\n\n组队作业项目");
        } else if (mainMenuExitItem && mainMenuExitItem->contains(mainMenuExitItem->mapFromScene(scenePos))) {
            quitGame();
        }
        return;
    }

    // ========== 游戏内菜单点击 ==========
    if (isGameMenuActive) {
        if (gameMenuContinueItem && gameMenuContinueItem->contains(gameMenuContinueItem->mapFromScene(scenePos))) {
            hideGameMenu();
        } else if (gameMenuAboutItem && gameMenuAboutItem->contains(gameMenuAboutItem->mapFromScene(scenePos))) {
            QMessageBox::information(this, "关于", "Qt-Gaming 演示游戏\n版本 2.0\n\n组队作业项目");
        } else if (gameMenuExitItem && gameMenuExitItem->contains(gameMenuExitItem->mapFromScene(scenePos))) {
            quitGame();
        }
        return;
    }

    // ========== 点击右上角菜单按钮（仅游戏进行中） ==========
    if (gameMenuButton && gameMenuButton->contains(gameMenuButton->mapFromScene(scenePos))) {
        // 传送门附近禁止打开菜单
        if (isNearPortal()) {
            // 显示短暂提示
            QGraphicsSimpleTextItem *tip = new QGraphicsSimpleTextItem("传送门附近无法打开菜单");
            tip->setBrush(Qt::yellow);
            tip->setPos(scenePos.x() - 80, scenePos.y() - 30);
            tip->setZValue(10000);
            scene->addItem(tip);
            QTimer::singleShot(1000, [tip]() {
                if (tip && tip->scene()) tip->scene()->removeItem(tip);
                delete tip;
            });
            return;
        }
        showGameMenu();
        return;
    }

    // 其他点击事件（例如游戏中点击地面等），交给基类处理（可选）
    QGraphicsView::mousePressEvent(event);
}

void Game::updateGame()
{
    if (isMainMenuActive || isGameMenuActive) return;
    if (gamePaused) return;

    // ========== 传送过渡：黑幕渐暗→传送→渐亮 ==========
    if (portalTransitionActive && player && scene) {
        if (!blackCurtain) {
            blackCurtain = new QGraphicsRectItem(scene->sceneRect());
            blackCurtain->setBrush(QBrush(Qt::black));
            blackCurtain->setPen(Qt::NoPen);
            blackCurtain->setZValue(5000);
            blackCurtain->setOpacity(0.0);
            scene->addItem(blackCurtain);
        }
        qreal cur = blackCurtain->opacity();

        if (portalTransitionPhase == 1) {
            // 渐暗
            qreal next = qMin(1.0, cur + FADE_SPEED / 100.0);
            blackCurtain->setOpacity(next);
            if (next >= 1.0) {
                // 全黑：传送 + 缩/放
                player->setPos(portalDest.x(), portalDest.y());
                // 仅改变体型，不改变等级/HP/MP/增强状态
                player->setTiny(portalShrink);
                centerOn(player);
                portalTransitionPhase = 2;
            }
        } else if (portalTransitionPhase == 2) {
            // 渐亮到完全透明
            qreal next = qMax(0.0, cur - FADE_SPEED / 100.0);
            blackCurtain->setOpacity(next);
            if (next <= 0.0) {
                portalTransitionPhase = 3;
                portalTransitionActive = false;
                portalCooldown = 120;  // 2秒冷却防反弹
                if (blackCurtain) { scene->removeItem(blackCurtain); delete blackCurtain; blackCurtain = nullptr; }
                qDebug() << "[Portal] Transition complete, curtain removed";
            }
        }
        // 过渡期间跳过所有正常逻辑
        return;
    }

    // 传送冷却倒计时
    if (portalCooldown > 0) portalCooldown--;

    // ========== 调试模式：纯地图查看，WASD 滚屏 ==========
    if (debugMapView) {
        if (!scene || !tileMap) return;
        const qreal SCROLL_SPEED = 8.0;
        qreal dx = 0, dy = 0;
        if (leftPressed)  dx = -SCROLL_SPEED;
        if (rightPressed) dx =  SCROLL_SPEED;
        if (upPressed)    dy = -SCROLL_SPEED;
        if (downPressed)  dy =  SCROLL_SPEED;
        if (dx != 0 || dy != 0) {
            QPointF center = mapToScene(viewport()->rect().center());
            center += QPointF(dx, dy);
            centerOn(center);
        }
        return;
    }

    // 地图切换期间玩家/地图可能为空
    if (!player || !tileMap || !scene) return;

    // ========== 突进平滑移动（pugong_rush）==========
    if (rushDashing && player) {
        player->setPos(player->pos() + rushStep);
        if (tileMap && tileMap->collidesWithWall(player->hitboxRect()))
            player->setPos(player->pos() - rushStep);
        rushFramesLeft--;
        if (rushFramesLeft <= 0) rushDashing = false;
    }

    // ========== 受伤定身倒计时 ==========
    if (stunTimer > 0) stunTimer--;

    // J 连击：窗口计时
    if (jPressTimer > 0) jPressTimer--;
    // J 连击播放中：一个接一个播放
    if (jPlaying && player) {
        if (!player->isCastingNow()) {
            jPlayIndex++;
            if (jPlayIndex >= jPressCount) {
                // 全部播完，恢复站立
                jPlaying = false;
                jPressCount = 0;
                jPlayIndex = 0;
                player->clearCastState();
            } else {
                QString gif;
                if (jPlayIndex == 0)      gif = ":/images/player_enhanced_pugong_bright.gif";
                else if (jPlayIndex == 1) gif = ":/images/player_enhanced_pugong2_bright.gif";
                else if (jPlayIndex == 2) gif = ":/images/player_enhanced_pugong3_bright.gif";
                else {
                    gif = ":/images/player_enhanced_pugong_rush_bright.gif";
                    // 平滑突进（类似K闪现）
                    rushDashing = true;
                    rushFramesLeft = 8;
                    rushStep = jRushDir * (96.0 / 8.0);  // 3格
                }
                int interval = (jPlayIndex == 3) ? 1 : 2;
                player->playCastAnimation(gif, interval, 77, true);
                // 每击火圈+伤害
                skillNormalAttack();
            }
        }
    }
    if (jPlaying || jPressTimer > 0) {
        if (!jPlaying) {} // still counting, allow movement? No, block during combo window too
    }

    // ========== 加速技能冷却 ==========
    if (speedBoostTimer > 0) {
        speedBoostTimer--;
        if (speedBoostTimer == 0 && player) player->setSpeed(4.0);  // 恢复原速
    }
    if (speedCooldownTimer > 0) speedCooldownTimer--;

    // ========== 闪现动画（优先处理）==========
    if (flashState.active && player) {
        player->setPos(player->pos() + flashState.step);
        flashState.framesLeft--;

        // 增强形态闪现：玩家身上播放 rush_jumb GIF（启动时已预加载）
        if (player->getEnhanced() && qAbs(flashState.bladeDir.x()) > 0.01) {
            if (!g_rushJumbFrames.isEmpty()) {
                int idx = (20 - flashState.framesLeft) % g_rushJumbFrames.size();
                QPixmap frame = g_rushJumbFrames[idx];
                if (flashState.bladeDir.x() < 0) frame = frame.transformed(QTransform::fromScale(-1,1), Qt::SmoothTransformation);
                player->setPixmap(frame);
                player->setTransform(QTransform());
            }
        } else {
            // 垂直闪现：白光残影
            QGraphicsEllipseItem *dot = new QGraphicsEllipseItem(-4, -4, 8, 8);
            dot->setPos(player->sceneBoundingRect().center());
            dot->setBrush(QBrush(QColor(255, 255, 255, 200)));
            dot->setPen(Qt::NoPen);
            scene->addItem(dot);
            QTimer::singleShot(200, [dot]() { delete dot; });
        }

        if (flashState.framesLeft <= 0) {
            // 闪现结束，校正到最终位置（动画由movie自动恢复）
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
        // 定身/J连击/突进期间禁止移动
        if (stunTimer > 0 || jPlaying || rushDashing) {
            player->move(false, false, false, false);
        } else {
            player->move(upPressed, downPressed, leftPressed, rightPressed);
            if (upPressed||downPressed||leftPressed||rightPressed) lastMoveDir = getCurrentDirectionVector();
        }
        // ========== 新增：水碰撞回退 ==========
        if (tileMap->collidesWithWater(player->hitboxRect())) {
            player->setPos(oldPos);                        // 回退到移动前
        }
        player->updateCastAnimation();
        centerOn(player);

        // ========== 子空间区域检测 ==========
        {
            int ts = tileMap->getTileWidth();
            QPointF hc = player->hitboxRect().center();
            int tx = static_cast<int>(hc.x()) / ts;
            int ty = static_cast<int>(hc.y()) / ts;
            int newId = 0;
            // garden   (204,10) 52×42 → ID=1
            if      (tx >= 204 && tx < 256 && ty >= 10 && ty < 52) newId = 1;
            // gateway  (159,35) 42×12 → ID=2
            else if (tx >= 159 && tx < 201 && ty >= 35 && ty < 47) newId = 2;
            // maze     (120,25) 32×22 → ID=3
            else if (tx >= 120 && tx < 152 && ty >= 25 && ty < 47) newId = 3;
            // secret   (241,58) 50×20 → ID=4
            else if (tx >= 241 && tx < 291 && ty >= 58 && ty < 78) newId = 4;
            if (newId != subspaceId) {
                int prevId = subspaceId;
                subspaceId = newId;
                qDebug() << "[Subspace] entered region" << subspaceId << "at tile" << tx << ty;

                // 雾：进入迷宫(region 3) → 目标 0.5，离开 → 目标 0（10s渐变）
                fogTargetOpacity = (newId == 3) ? 0.5 : 0.0;

                // === 树可见性 ===
                bool in24 = (newId == 2 || newId == 4);
                bool was24 = (prevId == 2 || prevId == 4);
                if (in24 && !was24) {
                    // 进入区域2/4：所有树几乎看不见
                    for (auto *t : treeOverlays) t->setOpacity(0.1);
                    for (auto *t : gatewayTrees) t->setOpacity(0.1);
                    for (auto *t : secretTrees) t->setOpacity(0.1);
                    if (gatewayRoof) gatewayRoof->setOpacity(0.0);
                    if (secretRoof) secretRoof->setOpacity(0.0);
                } else if (!in24 && was24) {
                    // 离开区域2/4：恢复
                    for (auto *t : treeOverlays) t->setOpacity(1.0);
                    for (auto *t : gatewayTrees) t->setOpacity(0.5);
                    for (auto *t : secretTrees) t->setOpacity(0.75);
                    if (gatewayRoof) gatewayRoof->setOpacity(1.0);
                    if (secretRoof) secretRoof->setOpacity(1.0);
                }
                // 区域4特殊：进入时 gatewayTrees→0.3, secretTrees→0.1
                if (newId == 4 && prevId != 4) {
                    for (auto *t : gatewayTrees) t->setOpacity(0.3);
                }
                // 区域1特殊
                if (newId == 1 && prevId != 1) {
                    for (auto *t : treeOverlays) t->setOpacity(1.0);
                } else if (prevId == 1 && newId != 1 && newId != 2 && newId != 4) {
                    for (auto *t : treeOverlays) t->setOpacity(0.4);
                }
            }
        }
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
    checkAndShowIntro();
    // 雾透明度 10s 线性渐变
    if (fogOverlay) {
        static constexpr qreal FOG_LERP_STEP = 0.5 / 600.0; // 0→0.5 需10s(600帧)
        qreal cur = fogOverlay->opacity();
        qreal tgt = fogTargetOpacity;
        if (qAbs(cur - tgt) < FOG_LERP_STEP)
            fogOverlay->setOpacity(tgt);
        else if (cur < tgt)
            fogOverlay->setOpacity(cur + FOG_LERP_STEP);
        else
            fogOverlay->setOpacity(cur - FOG_LERP_STEP);
    }

    updateMinimap();            // ← 更新小地图红点位置
    updateDiamonds();           // ← 钻石动画与碰撞
    updatePetals();             // ← 梅花瓣/暴风雪粒子
    if (currentMapPath.contains("lianda")) updateDangerZones();  // 西南联大地图机制
    updateGameMenuButtonPosition();
    updateSkillBarPosition();
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
            createExplosion(p->sceneBoundingRect().center());
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

void Game::createExplosion(QPointF centerPos, int size)
{
    qDebug() << "[Bomb] createExplosion called, frames=" << g_bombFrames.size() << "size=" << size;
    if (!scene || g_bombFrames.isEmpty()) return;

    // 创建爆炸动画项
    QGraphicsPixmapItem *bombItem = new QGraphicsPixmapItem();
    bombItem->setPixmap(g_bombFrames[0]);
    bombItem->setZValue(999999);  // 最顶层，仅低于变身动画(10000000)
    bombItem->setTransformationMode(Qt::SmoothTransformation);
    // 按指定尺寸缩放（默认144，危险区域传480与红圈一致）
    qreal s = static_cast<qreal>(size) / g_bombFrames[0].width();
    bombItem->setTransform(QTransform::fromScale(s, s));
    bombItem->setPos(centerPos.x() - size/2, centerPos.y() - size/2);
    scene->addItem(bombItem);

    // 手动逐帧播放
    int *frameIdx = new int(0);
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [timer, bombItem, frameIdx]() {
        (*frameIdx)++;
        if (*frameIdx >= g_bombFrames.size()) {
            timer->stop();
            if (bombItem->scene()) bombItem->scene()->removeItem(bombItem);
            delete bombItem;
            delete frameIdx;
            timer->deleteLater();
            return;
        }
        bombItem->setPixmap(g_bombFrames[*frameIdx]);
    });
    timer->start(8);  // ~8ms/帧，约0.38秒播完
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
    qreal flashDistance = 200.0; // 最大闪现距离
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
    const int FLASH_FRAMES = 20;
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

    // ========== 1. 九宫格攻击范围 ==========
    QPointF playerCenter = player->sceneBoundingRect().center();
    QRectF attackRect(playerCenter.x() - 48.0, playerCenter.y() - 48.0, 96.0, 96.0);

    // ========== 2. 普攻 GIF 特效（预加载帧，持续 2 秒后消失）==========
    QGraphicsPixmapItem *hitItem = new QGraphicsPixmapItem();
    hitItem->setTransformationMode(Qt::SmoothTransformation);
    hitItem->setZValue(50);
    scene->addItem(hitItem);
    hitItem->setPos(playerCenter.x() - 72, playerCenter.y() - 72);

    if (!g_fireHitFrames.isEmpty()) {
        hitItem->setPixmap(g_fireHitFrames[0]);
        int *frameIdx = new int(0);
        int *elapsed = new int(0);
        QTimer *hitTimer = new QTimer(this);
        connect(hitTimer, &QTimer::timeout, [hitTimer, hitItem, frameIdx, elapsed]() {
            (*elapsed)++;
            (*frameIdx) = (*frameIdx + 1) % g_fireHitFrames.size();
            hitItem->setPixmap(g_fireHitFrames[*frameIdx]);
            if (*elapsed >= 250) {  // 250×8ms = 2s
                hitTimer->stop();
                if (hitItem->scene()) hitItem->scene()->removeItem(hitItem);
                delete hitItem;
                delete frameIdx;
                delete elapsed;
                hitTimer->deleteLater();
            }
        });
        hitTimer->start(8);
    }

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
    updateGameMenuButtonPosition();  // 缩放后重新计算位置
}

void Game::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateGameMenuButtonPosition();
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

    // 实时坐标显示（小地图下方）
    minimapPosText = new QGraphicsSimpleTextItem();
    minimapPosText->setBrush(QBrush(QColor(0, 255, 0)));
    QFont f = minimapPosText->font();
    f.setPointSize(9);
    f.setBold(true);
    minimapPosText->setFont(f);
    minimapPosText->setZValue(1001);
    scene->addItem(minimapPosText);
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

    // 实时坐标：用碰撞框中心（即角色实际所站 tile）
    if (minimapPosText) {
        int ts = tileMap->getTileWidth();
        QPointF hc = player->hitboxRect().center();
        int tileX = static_cast<int>(hc.x()) / ts;
        int tileY = static_cast<int>(hc.y()) / ts;
        qreal mmH = mapH * scale;
        minimapPosText->setText(QString("x:%1 y:%2").arg(tileX).arg(tileY));
        minimapPosText->setPos(mmPos.x(), mmPos.y() + mmH + 4);
    }
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
            // 只在四个区域内生成
            bool inRegion = (gx >= 204 && gx < 256 && gy >= 10 && gy < 52)   // 1-garden
                         || (gx >= 159 && gx < 201 && gy >= 35 && gy < 47)   // 2-gateway
                         || (gx >= 120 && gx < 152 && gy >= 25 && gy < 47)   // 3-maze
                         || (gx >= 241 && gx < 291 && gy >= 58 && gy < 78);  // 4-secret
            if (!inRegion) continue;
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

    // 仅区域1(花园)飘花瓣、区域3(迷宫)暴风雪
    if (subspaceId != 1 && subspaceId != 3) {
        // 清理残留粒子
        for (int i = petals.size() - 1; i >= 0; --i) {
            Petal &p = petals[i];
            p.life = qMin(p.life, 10);  // 快速淡出
            p.item->setOpacity(p.item->opacity() * 0.7);
            if (p.life <= 0) {
                scene->removeItem(p.item); delete p.item;
                petals.removeAt(i);
            }
        }
        return;
    }

    int maxParticles = (subspaceId == 3) ? 500 : 300;       // 暴风雪满屏覆盖
    int spawnPerTick  = (subspaceId == 3) ? 12 : 4;         // 暴风雪每帧12粒

    // 在视口全宽范围内生成
    QPointF vpTL = mapToScene(0, 0);
    QPointF vpBR = mapToScene(viewport()->width(), viewport()->height());
    qreal vpW = vpBR.x() - vpTL.x();
    qreal vpH = vpBR.y() - vpTL.y();

    static int spawnTick = 0;
    spawnTick++;
    if (spawnTick >= 1 && petals.size() < maxParticles) {
        spawnTick = 0;
        for (int n = 0; n < spawnPerTick && petals.size() < maxParticles; n++) {
            bool isSnow = (subspaceId == 3);
            auto *leaf = new QGraphicsEllipseItem(-2, -5, 4, 10);
            if (isSnow) {
                // 暴风雪：细长椭圆，45°倾斜对齐下落方向
                qreal w = 2 + QRandomGenerator::global()->bounded(3);    // 2~4px 宽
                qreal h = 6 + QRandomGenerator::global()->bounded(8);    // 6~13px 长
                leaf->setRect(-w/2, -h/2, w, h);
                leaf->setRotation(135);  // 椭圆长轴对齐45°飞行方向（顺方向）
                int wh = 210 + QRandomGenerator::global()->bounded(45);
                leaf->setBrush(QBrush(QColor(wh, wh, wh, 230)));
                leaf->setPen(Qt::NoPen);
            } else {
                bool isFlower = (QRandomGenerator::global()->bounded(2) == 0);
                if (isFlower) {
                    leaf->setBrush(QBrush(QColor(255, 180 + QRandomGenerator::global()->bounded(60), 200 + QRandomGenerator::global()->bounded(40), 200)));
                    leaf->setPen(QPen(QColor(255, 150, 180, 150), 1));
                } else {
                    int g = 180 + QRandomGenerator::global()->bounded(75);
                    leaf->setBrush(QBrush(QColor(60, g, 30, 200)));
                    leaf->setPen(QPen(QColor(80, g-20, 40, 140), 1));
                }
            }
            // 雪从左侧出现，花瓣视口全宽
            qreal startX, startY;
            if (isSnow) {
                startX = vpTL.x() + QRandomGenerator::global()->bounded((int)vpW); // 全屏宽随机
                startY = vpTL.y() + QRandomGenerator::global()->bounded((int)vpH); // 全屏高随机
            } else {
                startX = vpTL.x() + QRandomGenerator::global()->bounded((int)vpW);
                startY = vpTL.y() - QRandomGenerator::global()->bounded((int)vpH * 0.3);
            }
            leaf->setPos(startX, startY);
            leaf->setZValue(10000);
            scene->addItem(leaf);

            Petal p;
            p.item = leaf;
            if (isSnow) {
                p.vx = 2.0 + QRandomGenerator::global()->bounded(4);    // 1/4速 2~6 px/帧
                p.vy = 2.0 + QRandomGenerator::global()->bounded(4);    // 45°斜向下
                p.life = 200 + QRandomGenerator::global()->bounded(100); // 3.3~5秒（1/4速）
            } else {
                p.vx = -(0.3 + QRandomGenerator::global()->bounded(15) / 10.0);
                p.vy = 0.3 + QRandomGenerator::global()->bounded(12) / 10.0;
                p.rotation = QRandomGenerator::global()->bounded(360);
                p.life = 500 + QRandomGenerator::global()->bounded(300);
            }
            p.rotation = 0;
            petals.append(p);
        }
    }

    // 更新所有粒子
    bool isSnow = (subspaceId == 3);
    for (int i = petals.size() - 1; i >= 0; --i) {
        Petal &p = petals[i];
        p.life--;
        if (!isSnow) {
            p.rotation += 0.8;
            p.vx += qCos(p.rotation * M_PI / 180.0) * 0.015;
        }
        // 雪：直线运动，无旋转无摇摆

        qreal nx = p.item->x() + p.vx;
        qreal ny = p.item->y() + p.vy;
        p.item->setPos(nx, ny);
        if (!isSnow) p.item->setRotation(p.rotation);

        if (p.life < 20) {
            QColor c = p.item->brush().color();
            c.setAlpha(c.alpha() * p.life / 20);
            p.item->setBrush(QBrush(c));
        }
        bool outOfBounds = isSnow ? (nx > vpBR.x() + 40)              // 雪飞出右侧
                                  : (ny > vpBR.y() + 40);             // 花瓣出底部
        if (p.life <= 0 || outOfBounds) {
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
        if (num == 0) { loadMap(":/maps/new_school_map.tmj", true); qDebug() << "Admin: → new_school_map spawn"; }
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

bool Game::isNearPortal(int expandTiles) const
{
    if (!player || !tileMap) return false;
    int ts = tileMap->getTileWidth();
    QRectF nearRect = player->hitboxRect().adjusted(-ts * expandTiles, -ts * expandTiles,
                                                    ts * expandTiles, ts * expandTiles);
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
    int newInterval = qMax(60, 180 - (playerLevel - 1) * 12);

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
    transformItem->setZValue(10000000);  // 绝对顶层，高于bomb(999999)
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
    clearAllSkillProjectiles();

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
        // 玩家显示 64×64，碰撞框为右下角 32×32，中心偏移 (48, 48)
        const int COLLISION_CENTER_OFFSET_X = 48;
        const int COLLISION_CENTER_OFFSET_Y = 48;
        QPointF basePos = targetCenter - QPointF(COLLISION_CENTER_OFFSET_X, COLLISION_CENTER_OFFSET_Y);
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
        QString oldMapPath = currentMapPath;
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

        // ========== 判断是否需要等待介绍 ==========
        if (oldMapPath.contains("new_school_map") && 
            (newMapPath.contains("chamber1") || newMapPath.contains("lianda") || newMapPath.contains("Weiming_lake"))) {
            if (!introShownForTargetMaps.contains(newMapPath)) {
                waitingForIntro = true;
                waitingIntroMap = newMapPath;
                qDebug() << "[Intro] Set waiting for" << newMapPath;
            }
        }

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

    bool onFire = tileMap->collidesWithFire(playerRect);

    if (onFire) {
        fireDamageCounter++;
        if (fireDamageCounter >= FIRE_DAMAGE_INTERVAL) {
            fireDamageCounter = 0;
            player->takeDamage(5);
            qDebug() << "Fireland damage! HP:" << player->getHp();
        }
    } else {
        fireDamageCounter = 0;
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
            if (keyCount >= 1.0f) {
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
                break;  // 一次只开一扇门（避免同时开多扇）
            }
        }
    }
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

// ========== 西南联大机制：定时轰炸区域 ==========
void Game::updateDangerZones()
{
    if (!player || !scene) return;

    // 每15秒在玩家位置生成一个危险区域
    dangerSpawnTimer++;
    if (dangerSpawnTimer >= DANGER_INTERVAL) {
        dangerSpawnTimer = 0;
        DangerZone dz;
        dz.pos = player->sceneBoundingRect().center();
        dz.lifetime = DANGER_LIFETIME;
        dz.radius = 240;  // 15 tiles / 2
        // 随机漂移方向
        qreal angle = QRandomGenerator::global()->bounded(360) * M_PI / 180.0;
        dz.driftVel = QPointF(qCos(angle) * 0.5, qSin(angle) * 0.5);

        int r = dz.radius;
        // 红色圆
        dz.circle = new QGraphicsEllipseItem(-r, -r, r*2, r*2);
        dz.circle->setPen(QPen(QColor(255, 40, 40, 220), 3));
        dz.circle->setBrush(QBrush(QColor(255, 0, 0, 30)));
        dz.circle->setPos(dz.pos);
        dz.circle->setZValue(50);
        scene->addItem(dz.circle);

        // 叉号 X：两条对角线
        dz.cross1 = new QGraphicsLineItem(-r*0.7, -r*0.7, r*0.7, r*0.7);
        dz.cross1->setPen(QPen(QColor(255, 40, 40, 220), 3));
        dz.cross1->setPos(dz.pos);
        dz.cross1->setZValue(51);
        scene->addItem(dz.cross1);

        dz.cross2 = new QGraphicsLineItem(r*0.7, -r*0.7, -r*0.7, r*0.7);
        dz.cross2->setPen(QPen(QColor(255, 40, 40, 220), 3));
        dz.cross2->setPos(dz.pos);
        dz.cross2->setZValue(51);
        scene->addItem(dz.cross2);

        dangerZones.append(dz);
        qDebug() << "[DangerZone] Spawned at" << dz.pos;
    }

    // 更新所有危险区域
    for (int i = dangerZones.size() - 1; i >= 0; --i) {
        DangerZone &dz = dangerZones[i];
        dz.lifetime--;

        // 缓慢漂移
        dz.pos += dz.driftVel;
        if (dz.circle) dz.circle->setPos(dz.pos);
        if (dz.cross1) dz.cross1->setPos(dz.pos);
        if (dz.cross2) dz.cross2->setPos(dz.pos);

        // 时间到：爆炸
        if (dz.lifetime <= 0) {
            // 爆炸效果
            createExplosion(dz.pos, dz.radius * 2);  // 与红圈同大小 (480px)
            // 判定玩家是否在范围内：扣20%最大血量
            QRectF playerRect = player->hitboxRect();
            QRectF dangerRect(dz.pos.x() - dz.radius, dz.pos.y() - dz.radius, dz.radius*2, dz.radius*2);
            if (playerRect.intersects(dangerRect)) {
                int dmg = player->getMaxHp() * 0.2;
                if (dmg < 1) dmg = 1;
                player->takeDamage(dmg);
                qDebug() << "[DangerZone] HIT! Player took" << dmg << "damage";
            }
            // 清理
            delete dz.circle; delete dz.cross1; delete dz.cross2;
            dangerZones.removeAt(i);
        }
    }
}

// ==================== 交互物系统（R键阅读）====================

bool Game::isNearInteraction() const
{
    if (!player || !tileMap) return false;

    int tileSize = tileMap->getTileWidth();
    QPoint playerGrid(
        static_cast<int>(player->x()) / tileSize,
        static_cast<int>(player->y()) / tileSize
        );

    for (const InteractionSpot &spot : interactionSpots) {
        if (qAbs(playerGrid.x() - spot.gridPos.x()) <= 2 &&
            qAbs(playerGrid.y() - spot.gridPos.y()) <= 2) {
            return true;
        }
    }
    return false;
}

int Game::getCurrentInteractionType() const
{
    if (!player || !tileMap) return 0;

    int tileSize = tileMap->getTileWidth();
    QPoint playerGrid(
        static_cast<int>(player->x()) / tileSize,
        static_cast<int>(player->y()) / tileSize
        );

    for (const InteractionSpot &spot : interactionSpots) {
        if (qAbs(playerGrid.x() - spot.gridPos.x()) <= 2 &&
            qAbs(playerGrid.y() - spot.gridPos.y()) <= 2) {
            return spot.type;
        }
    }
    return 0;
}

void Game::showInteractionDialog(int type)
{
    // 暂停游戏
    if (gameTimer) gameTimer->stop();
    gamePaused = true;

    // 清除所有移动按键状态
    upPressed = downPressed = leftPressed = rightPressed = false;
    // 处理积压的按键事件（确保状态彻底清零）
    QApplication::processEvents();

    QString title;
    QString content;

    if (type == 2) {
        title = "北京全体学界通告";
        content =
            "外争主权，内除国贼！\n\n"
            "中国的土地可以征服而不可以断送！\n"
            "中国的人民可以杀戮而不可以低头！\n\n"
            "国亡了！同胞起来呀！\n\n"
            "…… \n\n"
            "—— 罗家伦 起草，1919年5月4日";
    } else if (type == 1) {
        title = "残损的书刊";
        content =
            "书籍已经残损，然而一些文字仍清晰可见\n"
            "一篇小说吸引了你的注意……\n\n"
            "……\n"
            "我翻开历史一查……\n"
            "每页都写着‘仁义道德’几个字……\n"
            "……从字缝里看出字来……\n"
            "满本都写着两个字是 \n"
            " 吃人 \n"
            "……\n\n"
            "—— 作者的姓名已然看不清晰，但他犀利而深邃的思想令你倾佩";
    }else if (type == 3) {
        // 民主墙：随机显示 5 篇文本之一
        title = "📜 西南联大·民主墙";

        // 5 篇文本数组
        QStringList contents;
        contents <<
            // 文本1：倒孔运动
            "孔贼不死，贪污不止！\n\n"
            "打倒发国难财的孔祥熙！\n\n"
            "1941年，香港沦陷，国民政府派飞机救援，"
            "机上却装着孔祥熙家属的洋狗、箱笼和佣人。\n"
            "前方吃紧，后方紧吃，此贼不除，国将不国！\n\n"
            "—— 1942年1月，西南联大民主墙"

                 <<
            // 文本2：反对一党专政
            "一党专政必须终止！\n\n"
            "十多年来，我国政权实际上操于介石先生一人之手。\n"
            "独裁必然导致腐败，腐败必然导致亡国。\n\n"
            "我们要的是民主的、自由的、联合的政府！\n\n"
            "—— 张奚若、闻一多、朱自清等十教授联名电文，1945年"

                 <<
            // 文本3：学术自由
            "教育独立，学术自由！\n\n"
            "教育部统一课程、统一教材，甚至统一考试，\n"
            "把'党义'强加为必修课，这是对大学的奴化！\n\n"
            "大学不是政府的传声筒，\n"
            "而是独立思想的堡垒！\n\n"
            "—— 西南联大教务会议致教育部函，1940年"

                 <<
            // 文本4：宣传抗日
            "八路军新四军浴血抗战！\n\n"
            "百团大战歼灭日伪军数万，\n"
            "敌后战场牵制了日军大半兵力。\n\n"
            "然而，这些英勇的战士却得不到弹药补给，\n"
            "甚至被污名为'叛军'。\n\n"
            "真相何在？公道何在？\n\n"
            "—— 壁报《群声》转载《新华日报》，1941年"

                 <<
            // 文本5：青春与希望
            "我们是在黑暗中摸索的一代，\n"
            "但我们相信，黎明终将到来。\n\n"
            "民主墙上的每一篇文章，每一行字，\n"
            "都是我们不甘沉默的呐喊。\n\n"
            "愿后人记得：\n"
            "在这片土地上，曾有一群青年，\n"
            "用热血和笔杆，敲响了旧世界的丧钟。\n\n"
            "—— 佚名，西南联大民主墙";

        // 随机选择一篇
        int r = QRandomGenerator::global()->bounded(contents.size());
        content = contents[r];
    }
    else {
        title = "📜 历史文献";
        content = "暂无内容。";
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setMinimumSize(450, 350);
    dialog.setStyleSheet(
        "QDialog { background-color: #f5e6c8; }"
        "QTextEdit { background-color: #fff8e7; color: black; font-family: 'Microsoft YaHei'; font-size: 14px; }"
    );

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setPlainText(content);
    textEdit->setReadOnly(true);
    textEdit->setFrameShape(QFrame::NoFrame);
    layout->addWidget(textEdit);

    QPushButton *okButton = new QPushButton("知道了", &dialog);
    okButton->setStyleSheet(
        "QPushButton { background-color: #8B4513; color: white; padding: 8px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #A0522D; }"
        );
    layout->addWidget(okButton);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(&dialog, &QDialog::finished, this, [this]() {
        gamePaused = false;
    });

    dialog.exec();

    // 对话框关闭后，再次清除按键状态
    upPressed = downPressed = leftPressed = rightPressed = false;
    QApplication::processEvents();

    // 恢复游戏循环
    gamePaused = false;
    if (gameTimer) gameTimer->start(16);
}

void Game::clearAllSkillProjectiles()
{
    // 清理 Projectile（I 技能火球，3级+）
    for (Projectile *p : projectiles) {
        if (p->scene()) p->scene()->removeItem(p);
        delete p;
    }
    projectiles.clear();

    // 清理 SimpleProjectile（I 技能红色椭圆弹，1-2级）
    for (SimpleProjectile *sp : simpleProjectiles) {
        if (sp->scene()) sp->scene()->removeItem(sp);
        delete sp;
    }
    simpleProjectiles.clear();

    // 清理 BlueProjectile（N 键月牙弹）
    for (BlueProjectile *bp : blueProjectiles) {
        if (bp->scene()) bp->scene()->removeItem(bp);
        delete bp;
    }
    blueProjectiles.clear();

    // 清理 TriangleProjectile（H 键破空梭）
    for (TriangleProjectile *tp : triangleProjectiles) {
        if (tp->scene()) tp->scene()->removeItem(tp);
        delete tp;
    }
    triangleProjectiles.clear();

    // 清理 BladeWave（K 技能 1级刀浪）
    for (BladeWave *bw : bladeWaves) {
        if (bw->scene()) bw->scene()->removeItem(bw);
        delete bw;
    }
    bladeWaves.clear();

    // 清理 DaolangWave（K 技能 2级+ GIF 刀浪）
    for (DaolangWave *dw : daolangWaves) {
        if (dw->scene()) dw->scene()->removeItem(dw);
        delete dw;
    }
    daolangWaves.clear();

    qDebug() << "[Portal] All skill projectiles cleared.";
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