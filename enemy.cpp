#include "enemy.h"
#include "tilemap.h"
#include "game.h"
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>
#include <QDebug>
#include <QRandomGenerator>
#include <QImageReader>
#include <QStringList>
#include <algorithm>

// ========== 全局 monster 帧缓存（预提取，所有敌人共享） ==========
static QVector<QVector<QPixmap>> g_monsterFrames;  // 每种 monster 的帧序列
static bool g_monsterLoaded = false;

static const QStringList g_monsterPaths = {
    ":/images/monster_bomb.gif",
    ":/images/monster_boss_hit.gif",
    ":/images/monster_cool_phone.gif",
    ":/images/monster_paper.gif",
    ":/images/monster_pencil_right.gif",
    ":/images/monster_terrible_phone.gif",
    ":/images/monster_xuanzhi.gif",
    ":/images/monster_zhannindeguang.gif"
};

void preloadMonsterFrames()
{
    if (g_monsterLoaded) return;
    g_monsterLoaded = true;

    for (const QString &path : g_monsterPaths) {
        QVector<QPixmap> frames;
        QImageReader reader(path);
        reader.setAutoDetectImageFormat(true);
        int count = 0;
        while (reader.canRead()) {
            QImage img = reader.read();
            if (!img.isNull()) {
                // 每 2 帧取 1 帧，减少内存
                if (count % 2 == 0) {
                    frames.append(QPixmap::fromImage(img));
                }
                count++;
            }
        }
        if (!frames.isEmpty()) {
            g_monsterFrames.append(frames);
        } else {
            qDebug() << "preloadMonsterFrames: failed to load" << path;
        }
    }
    qDebug() << "preloadMonsterFrames: loaded" << g_monsterFrames.size() << "monster types";
}

// ============================================================================
//  EnemyProjectile 怪物炮弹
// ============================================================================

EnemyProjectile::EnemyProjectile(QPointF startPos, QPointF velocity, int damage,
                                 QGraphicsScene *scene, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent),
      velocity(velocity),
      damage(damage),
      m_scene(scene)
{
    // 紫色炮弹：圆形，半径 5px
    setRect(-5, -5, 10, 10);
    setPos(startPos);
    setZValue(6);  // 在敌人（Z=5）之上
    setBrush(QBrush(QColor(160, 32, 240)));      // 紫色填充
    setPen(QPen(QColor(220, 100, 255), 2));      // 亮紫边框
    if (scene) {
        scene->addItem(this);
    }
}

EnemyProjectile::~EnemyProjectile()
{
    // QGraphicsItem 析构自动从场景移除
}

bool EnemyProjectile::update(TileMap *tileMap)
{
    // 移动
    setPos(pos() + velocity);

    // 墙壁碰撞
    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    // 场景边界
    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  Enemy 怪物
// ============================================================================

Enemy::Enemy(TileMap *tileMap, QGraphicsScene *scene, QPointF startPos, Game *game, int initialHp)
    : QGraphicsPixmapItem(),
      tileMap(tileMap),
      m_scene(scene),
      game(game),
      speed(1.5),
      moveCounter(0),
      moveInterval(60),   // 每 60 帧（约 1 秒）换一次方向
      attackCounter(0),
      attackInterval(120), // 每 120 帧（约 2 秒）攻击一次
      hp(initialHp),
      maxHp(initialHp),
      dead(false)
{
    // 随机选择一种 monster 帧缓存（预提取，零运行时解码开销）
    if (!g_monsterFrames.isEmpty()) {
        monsterType = QRandomGenerator::global()->bounded(g_monsterFrames.size());
        animFrame = QRandomGenerator::global()->bounded(g_monsterFrames[monsterType].size());
        const QPixmap &firstFrame = g_monsterFrames[monsterType][animFrame];
        qreal sx = 64.0 / firstFrame.width();
        qreal sy = 64.0 / firstFrame.height();
        setTransform(QTransform::fromScale(sx, sy));
        setPixmap(firstFrame);
    } else {
        // 回退：使用 minion.png
        QPixmap pixmap(":/images/minion.png");
        if (!pixmap.isNull()) {
            qreal sx = 64.0 / pixmap.width();
            qreal sy = 64.0 / pixmap.height();
            setTransform(QTransform::fromScale(sx, sy));
            setPixmap(pixmap);
        }
    }

    setPos(startPos);
    setZValue(5);  // 在地形（Z≤0）和玩家（Z=2）之上
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    setTransformationMode(Qt::SmoothTransformation);

    if (scene) {
        scene->addItem(this);

        // 创建血条（位于怪物头顶）
        hpBarBg = new QGraphicsRectItem(-16, -10, 32, 4);
        hpBarBg->setBrush(QBrush(Qt::darkGray));
        hpBarBg->setPen(Qt::NoPen);
        scene->addItem(hpBarBg);

        hpBarFg = new QGraphicsRectItem(-16, -10, 32, 4);
        hpBarFg->setBrush(QBrush(Qt::red));
        hpBarFg->setPen(Qt::NoPen);
        scene->addItem(hpBarFg);
    }

    randomizeDirection();
}

Enemy::~Enemy()
{
    // 删除血条
    if (hpBarBg) {
        delete hpBarBg;
        hpBarBg = nullptr;
    }
    if (hpBarFg) {
        delete hpBarFg;
        hpBarFg = nullptr;
    }
}

void Enemy::randomizeDirection()
{
    // 从 8 个方向中随机选一个
    int dir = QRandomGenerator::global()->bounded(8);
    switch (dir) {
        case 0: moveDir = QPointF(1, 0); break;
        case 1: moveDir = QPointF(-1, 0); break;
        case 2: moveDir = QPointF(0, 1); break;
        case 3: moveDir = QPointF(0, -1); break;
        case 4: moveDir = QPointF(0.707, -0.707); break;
        case 5: moveDir = QPointF(0.707, 0.707); break;
        case 6: moveDir = QPointF(-0.707, -0.707); break;
        case 7: moveDir = QPointF(-0.707, 0.707); break;
    }
}

void Enemy::updateMonsterFrame()
{
    if (monsterType < 0 || monsterType >= g_monsterFrames.size()) return;
    const QVector<QPixmap> &frames = g_monsterFrames[monsterType];
    if (frames.isEmpty()) return;

    // 每 4 帧切换一次（约 15fps），降低动画速度
    animTick++;
    if (animTick >= 4) {
        animTick = 0;
        animFrame = (animFrame + 1) % frames.size();
        const QPixmap &f = frames[animFrame];
        qreal sx = 64.0 / f.width();
        qreal sy = 64.0 / f.height();
        setTransform(QTransform::fromScale(sx, sy));
        setPixmap(f);
    }
}

void Enemy::update()
{
    if (dead) return;

    // ========== 随机游走 ==========
    moveCounter++;
    if (moveCounter >= moveInterval) {
        moveCounter = 0;
        randomizeDirection();
    }

    QPointF oldPos = pos();
    setPos(oldPos + QPointF(moveDir.x() * speed, moveDir.y() * speed));

    // 撞墙回退并换方向
    if (tileMap && tileMap->collidesWithWall(this)) {
        setPos(oldPos);
        randomizeDirection();
    }

    // 更新怪兽帧动画
    updateMonsterFrame();

    // 更新血条位置（跟随怪物）
    if (hpBarBg) hpBarBg->setPos(pos());
    if (hpBarFg) hpBarFg->setPos(pos());

    // ========== 攻击计时 ==========
    attackCounter++;
    if (attackCounter >= attackInterval) {
        attackCounter = 0;
        tryAttack();
    }
}

void Enemy::tryAttack()
{
    if (!game || !m_scene) return;

    qreal bulletSpeed = 5.0;
    int damage = 10;

    // 从 8 个方向中随机选 3 个不重复的方向
    QVector<int> allDirs = {0, 1, 2, 3, 4, 5, 6, 7};
    std::shuffle(allDirs.begin(), allDirs.end(), *QRandomGenerator::global());

    QPointF center = sceneBoundingRect().center();  // 用变换后的实际位置

    for (int i = 0; i < 3; ++i) {
        QPointF dir;
        switch (allDirs[i]) {
            case 0: dir = QPointF(1, 0); break;
            case 1: dir = QPointF(-1, 0); break;
            case 2: dir = QPointF(0, 1); break;
            case 3: dir = QPointF(0, -1); break;
            case 4: dir = QPointF(0.707, -0.707); break;
            case 5: dir = QPointF(0.707, 0.707); break;
            case 6: dir = QPointF(-0.707, -0.707); break;
            case 7: dir = QPointF(-0.707, 0.707); break;
        }

        EnemyProjectile *ep = new EnemyProjectile(
            center,
            QPointF(dir.x() * bulletSpeed, dir.y() * bulletSpeed),
            damage,
            m_scene
        );
        game->addEnemyProjectile(ep);
    }
}

void Enemy::updateHpBar()
{
    if (hpBarFg && maxHp > 0) {
        qreal ratio = static_cast<qreal>(hp) / maxHp;
        if (ratio < 0) ratio = 0;
        qreal width = 32.0 * ratio;
        hpBarFg->setRect(-16, -10, width, 4);
    }
}

void Enemy::takeDamage(int dmg)
{
    if (dead) return;
    hp -= dmg;
    qDebug() << "Enemy took damage:" << dmg << "HP left:" << hp << "/" << maxHp;
    updateHpBar();
    if (hp <= 0) {
        hp = 0;
        dead = true;
        qDebug() << "Enemy defeated!";
        // 死亡时隐藏血条
        if (hpBarBg) hpBarBg->hide();
        if (hpBarFg) hpBarFg->hide();
    }
}
