#ifndef ENEMY_H
#define ENEMY_H

#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QObject>
#include <QPointF>
#include <QVector>
#include <QPixmap>

class QGraphicsScene;
class TileMap;
class Game;

/** 预加载所有 monster GIF 的帧缓存（在 Game 构造函数中调用一次） */
void preloadMonsterFrames();

/**
 * @brief EnemyProjectile 怪物炮弹类
 *
 * 怪物发射的紫色圆形炮弹，直线飞行，撞墙或击中玩家后消失。
 */
class EnemyProjectile : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT
public:
    EnemyProjectile(QPointF startPos, QPointF velocity, int damage,
                    QGraphicsScene *scene, QGraphicsItem *parent = nullptr);
    ~EnemyProjectile();

    /**
     * @brief 每帧更新炮弹状态
     * @param tileMap 地图指针（墙壁碰撞检测）
     * @return true  炮弹存活
     * @return false 炮弹死亡（撞墙/出界），需要被销毁
     */
    bool update(TileMap *tileMap);

    int getDamage() const { return damage; }

private:
    QPointF velocity;        // 速度向量（像素/帧）
    int damage;              // 伤害值
    QGraphicsScene *m_scene; // 场景指针（边界检测）
};

// ============================================================================
//  Enemy 怪物类
// ============================================================================

/**
 * @brief Enemy 怪物类
 *
 * 随机游走型怪物：
 * - 每隔一定时间随机改变移动方向
 * - 撞墙则回退并重新随机方向
 * - 每隔一定时间随机向 3 个方向发射紫色炮弹
 */
class Enemy : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    Enemy(TileMap *tileMap, QGraphicsScene *scene, QPointF startPos, Game *game, int initialHp = 50);
    ~Enemy();

    /** 每帧更新（移动、攻击计时） */
    void update();

    /** 受到伤害（预留接口） */
    void takeDamage(int dmg);

    void setAttackInterval(int interval) { attackInterval = interval; }

    int getHp() const { return hp; }
    bool isDead() const { return dead; }

private:
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    Game *game;

    qreal speed;          // 移动速度（像素/帧）
    QPointF moveDir;      // 当前移动方向（归一化向量）
    int moveCounter;      // 移动计时器（帧）
    int moveInterval;     // 换方向间隔（帧）
    int attackCounter;    // 攻击计时器（帧）
    int attackInterval;   // 攻击间隔（帧）

    int hp;               // 生命值
    int maxHp;            // 最大生命值
    bool dead;            // 是否已死亡

    QGraphicsRectItem *hpBarBg = nullptr;  // 血条背景（灰色）
    QGraphicsRectItem *hpBarFg = nullptr;  // 血条前景（红色）

    int monsterType = -1;                  // 选中的 monster 索引（-1 = 回退到 minion）
    int animFrame = 0;                     // 当前动画帧序号
    int animTick = 0;                      // 帧切换计时器

    void randomizeDirection(); // 随机化移动方向
    void tryAttack();            // 尝试发动攻击
    void updateHpBar();          // 更新血条显示
    void updateMonsterFrame();   // 更新怪兽帧动画
};

#endif // ENEMY_H
