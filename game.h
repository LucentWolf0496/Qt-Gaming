#ifndef GAME_H
#define GAME_H

#include <QGraphicsView>
#include <QTimer>
#include <QKeyEvent>
#include <QString>
#include <QVector>
#include <QList>
#include "maploader.h"   // 必须包含，因为使用了 Portal 结构体
#include "skill.h"
#include "enemy.h"
#include "tile.h"

class Player;
class TileMap;
class Projectile;
class SimpleProjectile;
class Enemy;
class EnemyProjectile;
class Spawner;
class Pet;
class QMovie;

class Game : public QGraphicsView
{
    Q_OBJECT
public:
    Game(QWidget *parent = nullptr);
    ~Game();

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void loadMap(const QString &mapFilePath, bool useStartPoint = true);

    /** 敌人注册的炮弹加入游戏管理列表 */
    void addEnemyProjectile(EnemyProjectile *ep);
    /** 巢穴生成的敌人加入游戏管理列表 */
    void addEnemy(Enemy *e);
    /** 获取当前场上敌人数量 */
    int getEnemyCount() const { return enemies.size(); }

// game.h 中的 private slots 部分
private slots:
    void updateGame();
    void checkPortal();
    void performTeleport(const Portal &portal);
    void onPlayerDied();   // 新增

private:
    QGraphicsScene *scene = nullptr;
    Player *player = nullptr;
    TileMap *tileMap = nullptr;
    QTimer *gameTimer = nullptr;

    bool upPressed, downPressed, leftPressed, rightPressed;
    QString currentMapPath;
    bool canTeleport;
    bool isTeleporting;

    int regenCounter = 0; // HP/MP 恢复计时器（每 60 帧恢复 1 点）

    // 视野缩放
    qreal zoomLevel = 1.0;
    static constexpr qreal ZOOM_STEP = 1.15;
    static constexpr qreal MIN_ZOOM = 0.1;   // 可看到整张地图
    static constexpr qreal MAX_ZOOM = 6.0;
    void applyZoom();

    // 闪现动画状态
    struct FlashState {
        bool active = false;
        QPointF step;       // 每帧移动量
        int framesLeft = 0; // 剩余帧数
        QPointF finalPos;   // 最终目标位置
        QPointF bladeDir;   // 刀浪方向（闪现结束时发射）
    };
    FlashState flashState;

    // ========== HUD 血蓝条 ==========
    QGraphicsRectItem *hudHpBg = nullptr;
    QGraphicsRectItem *hudHpFg = nullptr;
    QGraphicsRectItem *hudMpBg = nullptr;
    QGraphicsRectItem *hudMpFg = nullptr;
    QGraphicsRectItem *hudExpBg = nullptr;
    QGraphicsRectItem *hudExpFg = nullptr;
    QGraphicsSimpleTextItem *hudText = nullptr;
    QGraphicsSimpleTextItem *hudLevelText = nullptr;

    void createHud();
    void updateHud();

    // ========== 经验等级系统 ==========
    void onPlayerLevelUp(int newLevel);
    void applyLevel10Enhancement();
    void playTransformAnimation(); // 3级变身动画

    // ========== 宠物系统 ==========
    Pet *pet = nullptr;

    // ========== 技能系统 ==========
    /** 当前场景中所有活跃的流星粒子 */
    QVector<Projectile*> projectiles;
    /** 当前场景中所有活跃的简易子弹（1-2级红色椭圆） */
    QVector<SimpleProjectile*> simpleProjectiles;
    /** 当前场景中所有活跃的冰魄八荒子弹（N键普攻2） */
    QVector<BlueProjectile*> blueProjectiles;
    /** 当前场景中所有活跃的破空梭（H键单方向） */
    QVector<TriangleProjectile*> triangleProjectiles;
    /** 当前场景中所有活跃的刀浪（1级矩形） */
    QVector<BladeWave*> bladeWaves;
    /** 当前场景中所有活跃的GIF刀浪（2级+） */
    QVector<DaolangWave*> daolangWaves;
    /** 可被攻击的对象列表（Boss、小怪等），供碰撞检测使用 */
    QList<QGraphicsItem*> hittableItems;

    bool gamePaused = false;        // 变身动画期间暂停游戏
    bool explosionsEnabled = false; // 5级后启用爆炸效果
    bool fireBgEnabled = false;     // 5级后启用背后火焰
    QGraphicsPixmapItem *transformItem = nullptr;
    QMovie *transformMovie = nullptr;
    QGraphicsPixmapItem *fireBgItem = nullptr;
    int fireBgFrameIdx = 0;
    int fireBgTick = 0;

    /** 技能一：天火燎原（按 I 键触发，静止时才能使用） */
    void skillMeteorBurst();
    /** 普攻2：蓝色八方向月牙子弹（按 N 键触发，可边移动边发射） */
    void skillBlueBurst();
    /** 普攻3：单方向破空梭（按 H 键触发，朝移动方向） */
    void skillTriangleShot();
    /** 每帧更新所有流星粒子（移动、碰撞、清理） */
    void updateProjectiles();
    /** 每帧更新所有简易子弹（移动、碰撞、清理） */
    void updateSimpleProjectiles();
    /** 每帧更新所有冰魄八荒子弹（移动、碰撞、清理） */
    void updateBlueProjectiles();
    /** 每帧更新所有破空梭（移动、碰撞、清理） */
    void updateTriangleProjectiles();
    /** 在指定位置创建爆炸动画（3x3 tile 大小） */
    void createExplosion(QPointF centerPos);

    /** 技能二：瞬影浪斩（按 K 键触发） */
    void skillFlashBlade();
    /** 每帧更新所有刀浪（移动、碰撞、清理） */
    void updateBladeWaves();
    /** 每帧更新所有GIF刀浪（移动、碰撞、清理） */
    void updateDaolangWaves();

    /** 普攻：九重炎杀（按 J 键触发） */
    void skillNormalAttack();

    /** 技能三：玄武盾（按住 L 键激活，松开关闭） */
    void skillShieldActivate();
    void skillShieldDeactivate();
    void updateShieldPosition(); // 每帧跟随玩家
    QGraphicsEllipseItem *shieldItem = nullptr; // 玄武盾图形项
    bool shieldActive = false;                  // 玄武盾是否激活

    // ========== 受伤定身 ==========
    int stunTimer = 0;
    static const int STUN_DURATION = 12;  // 0.2秒 (60fps × 0.2)

    // ========== 加速技能 O ==========
    int speedBoostTimer = 0;
    int speedCooldownTimer = 0;
    static const int SPEED_BOOST_DURATION = 300;   // 5s
    static const int SPEED_COOLDOWN = 300;         // 5s

    // ========== 管理员模式 ==========
    bool adminMode = false;
    QString keyBuffer;
    void processAdminKey(int key);

    // ========== 调试：地图图层置顶 ==========
    bool debugMapView = false;

    // ========== 子空间区域ID：0=无, 1=garden, 2=gateway, 3=maze, 4=secret ==========
    int subspaceId = 0;

    // ========== 雾和树叠加层引用（用于运行时调整透明度）==========
    QGraphicsPixmapItem *fogOverlay = nullptr;
    qreal fogTargetOpacity = 0.0;            // 雾目标透明度（10s渐变）
    QVector<QGraphicsPixmapItem*> treeOverlays;

    // ========== 黑幕（区域过渡用）==========
    QGraphicsRectItem *blackCurtain = nullptr;
    int curtainOpacityTarget = 0;   // 目标透明度百分比 (0~100)

    // ========== 传送过渡状态 ==========
    bool portalTransitionActive = false;
    int  portalTransitionPhase = 0;  // 0=idle, 1=fadeOut, 2=teleported, 3=fadeIn→done
    int  portalTransitionTick = 0;
    QPointF portalDest;              // 传送目标像素坐标
    bool portalShrink = true;        // true=缩小(→1×1), false=恢复(→96×96)
    int portalCooldown = 0;          // 传送后冷却帧数，防止立即反弹
    static const int FADE_SPEED = 3; // 每帧透明度变化量

    // ========== 钻石系统 ==========
    struct Diamond {
        QGraphicsPixmapItem *item = nullptr;
        int type = 0;      // 0=红(补血), 1=蓝(补蓝), 2=紫(攻击翻倍)
    };
    QVector<Diamond> diamonds;
    int attackBuffTimer = 0;   // 攻击力翻倍剩余帧数
    QGraphicsSimpleTextItem *buffIndicator = nullptr;  // 紫钻头顶十字
    static const int ATTACK_BUFF_DURATION = 600;  // 10秒
    void spawnDiamonds();
    void updateDiamonds();
    void spawnCrossEffect(QPointF center, int colorType);  // 0=红,1=蓝,2=紫
    void spawnArrivalEffect(QPointF center);  // 传送/出生炫酷竖线激光
    bool isNearPortal() const;  // 玩家是否在传送门 2 格范围内
    int getBuffedDamage(int base) const {
        return (attackBuffTimer > 0) ? base * 2 : base;
    }

    /** 根据当前按键状态获取闪现/刀浪方向向量 */
    QPointF getCurrentDirectionVector();

    // ========== 敌人系统 ==========
    QVector<Enemy*> enemies;                // 所有活跃敌人
    QVector<EnemyProjectile*> enemyProjectiles; // 所有活跃敌人炮弹

    /** 每帧更新所有敌人（移动、攻击） */
    void updateEnemies();
    /** 每帧更新所有敌人炮弹（移动、碰撞、清理） */
    void updateEnemyProjectiles();

    // ========== 巢穴系统 ==========
    QVector<Spawner*> spawners;             // 所有活跃巢穴

    /** 每帧更新所有巢穴 */
    void updateSpawners();

    QVector<QRectF> fireRects;           // 火焰区域矩形列表
    int fireDamageCounter = 0;            // 火焰伤害计时器
    static const int FIRE_DAMAGE_INTERVAL = 30;  // 每30帧扣1血（0.5秒）

    void applyTerrainEffects();           // 应用地形效果（火焰、草地等）

        // ========== 钥匙系统 ==========
    float keyCount = 0.0f;                // 当前钥匙数量（支持0.25累加）
    QVector<Tile*> chests;                // 场景中所有宝箱
    QVector<Tile*> doors;                 // 场景中所有门

    /** 检查玩家与宝箱/门的交互 */
    void checkInteractions();
    /** 打开宝箱（增加钥匙，移除宝箱） */
    void openChest(Tile *chest);
    /** 更新 UI 上钥匙数量的显示 */
    void updateKeyDisplay();

    // HUD 钥匙文本（在已有的 HUD 成员附近添加）
    QGraphicsSimpleTextItem *hudKeyText = nullptr;

    /** 移除与指定门瓦片相连的所有门区域（BFS） */
    int removeDoorRegion(Tile *startDoor);

    // ========== 梅花瓣粒子 ==========
    struct Petal {
        QGraphicsEllipseItem *item;
        qreal vx, vy;     // 速度
        qreal rotation;    // 当前角度
        int life;
    };
    QVector<Petal> petals;
    void updatePetals();

    // ========== 旋转传送门 ==========
    QVector<QGraphicsPixmapItem*> portalSprites;
    int portalRotTick = 0;

    // ========== 小地图 ==========
    QGraphicsPixmapItem *minimapItem = nullptr;
    QGraphicsEllipseItem *minimapDot = nullptr;
    QGraphicsSimpleTextItem *minimapPosText = nullptr;  // 实时坐标
    void createMinimap();
    void updateMinimap();

    // ========== 菜单系统 ==========
    bool isMainMenuActive;          // 主菜单是否显示
    bool isGameMenuActive;          // 游戏内菜单是否显示

    QGraphicsSimpleTextItem *mainMenuStartItem;
    QGraphicsSimpleTextItem *mainMenuAboutItem;
    QGraphicsSimpleTextItem *mainMenuExitItem;

    QGraphicsPixmapItem *gameMenuButton;               // 右上角菜单按钮
    QGraphicsSimpleTextItem *gameMenuContinueItem;    // 游戏内菜单：继续
    QGraphicsSimpleTextItem *gameMenuAboutItem;       // 游戏内菜单：关于
    QGraphicsSimpleTextItem *gameMenuExitItem;        // 游戏内菜单：退出

    void setupEmptyScene();          // 清空场景并设置纯色背景（用于主菜单）
    void showMainMenu();             // 显示主菜单
    void hideMainMenu();             // 隐藏主菜单
    void startGame();                // 开始新游戏（加载地图）
    void quitGame();                 // 退出程序

    void createGameMenuButton();     // 在游戏场景右上角创建菜单按钮
    void showGameMenu();             // 显示游戏内菜单（并暂停游戏）
    void hideGameMenu();             // 隐藏游戏内菜单（恢复游戏）

    void updateGameMenuButtonPosition();  // 更新按钮位置跟随屏幕右上角

    // ========== 地图介绍对话框 ==========
    bool introShownForSchoolMap;           // 校史馆介绍是否已显示
    QSet<QString> introShownForTargetMaps; // 已显示过介绍的目标地图（存地图文件名）

    void checkAndShowIntro();              // 在 updateGame 中检测并显示介绍
    void showIntroDialog(const QString &mapKey); // 显示介绍对话框，参数为地图标识
    bool waitingForIntro = false;    // 是否等待玩家离开传送门以显示介绍
    QString waitingIntroMap;         // 等待显示介绍的地图路径
};

#endif // GAME_H