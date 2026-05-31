#ifndef SKILL_H
#define SKILL_H

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QObject>
#include <QPointF>
#include <QTransform>

class QGraphicsScene;
class TileMap;

/**
 * @brief Projectile 火炮投射物（一技能）
 *
 * 从玩家发出的火焰投射物，使用预加载的 GIF 帧缓存。
 * 8 个方向根据速度向量自动选择对应的变换帧（翻转/90°旋转，避免 45° 失真）。
 * 不再使用 QMovie，彻底消除定时器 + 信号槽的卡顿开销。
 */
class Projectile : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    Projectile(QPointF startPos, QPointF velocity, int damage,
               TileMap *tileMap, QGraphicsScene *scene,
               QGraphicsItem *parent = nullptr);
    ~Projectile();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    qreal distanceTraveled;
    qreal maxDistance;

    int dirIndex;      // 0-7 方向索引
    int frameIndex;    // 当前帧索引
    int frameTick;     // 帧切换计时器（每 N 游戏帧切一帧）
};

// ============================================================================
//  简易投射物（1-2 级红色椭圆子弹）
// ============================================================================

class SimpleProjectile : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT
public:
    SimpleProjectile(QPointF startPos, QPointF velocity, int damage,
                     TileMap *tileMap, QGraphicsScene *scene,
                     QGraphicsItem *parent = nullptr,
                     const QColor &color = QColor(255, 60, 0),
                     int radius = 6);
    ~SimpleProjectile();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    qreal distanceTraveled;
    qreal maxDistance;
};

// ============================================================================
//  三角形投射物（H键：单方向子弹）
// ============================================================================

class TriangleProjectile : public QObject, public QGraphicsPolygonItem
{
    Q_OBJECT
public:
    TriangleProjectile(QPointF startPos, QPointF velocity, int damage,
                       TileMap *tileMap, QGraphicsScene *scene,
                       QGraphicsItem *parent = nullptr);
    ~TriangleProjectile();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    qreal distanceTraveled;
    qreal maxDistance;
};

// ============================================================================
//  冰魄八荒投射物（N键普攻2）
// ============================================================================

class BlueProjectile : public QObject, public QGraphicsPathItem
{
    Q_OBJECT
public:
    BlueProjectile(QPointF startPos, QPointF velocity, int damage,
                   TileMap *tileMap, QGraphicsScene *scene,
                   QGraphicsItem *parent = nullptr);
    ~BlueProjectile();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    qreal distanceTraveled;
    qreal maxDistance;
};

// ============================================================================
//  二技能：刀浪（Blade Wave）
// ============================================================================

/** 提前预加载 Projectile 的 GIF 帧缓存，避免首次释放技能时卡顿 */
void preloadProjectileFrames();

class BladeWave : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    BladeWave(QPointF startPos, QPointF velocity, int damage,
              TileMap *tileMap, QGraphicsScene *scene,
              QGraphicsItem *parent = nullptr);
    ~BladeWave();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    int maxDistance;
    qreal distanceTraveled;
};

// ============================================================================
//  2级+ GIF刀浪（Daolang Wave）
// ============================================================================

class DaolangWave : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    DaolangWave(QPointF startPos, QPointF velocity, int damage,
                TileMap *tileMap, QGraphicsScene *scene,
                QGraphicsItem *parent = nullptr);
    ~DaolangWave();

    bool update();
    int getDamage() const { return damage; }

private:
    QPointF velocity;
    int damage;
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    qreal distanceTraveled;
    qreal maxDistance;
    int frameIdx;
    int frameTick;
    bool facingRight;
};

#endif // SKILL_H
