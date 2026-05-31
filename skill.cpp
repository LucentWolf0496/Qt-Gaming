#include "skill.h"
#include "tilemap.h"
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>
#include <QDebug>
#include <QtMath>
#include <QImageReader>

// ============================================================================
//  静态帧缓存：所有 Projectile 共用，启动时只加载一次
// ============================================================================

namespace {
    QVector<QVector<QPixmap>> g_projectileFrames; // [8 directions][frame index]
    bool g_framesLoaded = false;

    void doLoadProjectileFrames()
    {
        if (g_framesLoaded) return;
        g_framesLoaded = true;

        auto extractFrames = [](const QString &path) -> QVector<QPixmap> {
            QVector<QPixmap> result;
            QImageReader reader(path);
            reader.setAutoDetectImageFormat(true);
            while (reader.canRead()) {
                QImage img = reader.read();
                if (!img.isNull()) {
                    result.append(QPixmap::fromImage(img));
                }
            }
            if (result.isEmpty()) {
                qDebug() << "Failed to extract frames from:" << path;
            }
            return result;
        };

        QVector<QPixmap> rightFrames = extractFrames(":/images/fly_fire_right.gif");
        QVector<QPixmap> diagFrames  = extractFrames(":/images/fly_fire_left_down.gif");

        if (rightFrames.isEmpty() && diagFrames.isEmpty()) return;

        g_projectileFrames.resize(8);

        struct DirInfo {
            const QVector<QPixmap> *source;
            QTransform transform;
        };
        DirInfo dirInfos[8] = {
            { &rightFrames, QTransform() },                              // 0: 右
            { &diagFrames,  QTransform().rotate(180) },                  // 1: 右上
            { &rightFrames, QTransform().rotate(90) },                   // 2: 下
            { &diagFrames,  QTransform::fromScale(-1, 1) },              // 3: 右下
            { &rightFrames, QTransform::fromScale(-1, 1) },              // 4: 左
            { &diagFrames,  QTransform() },                              // 5: 左下
            { &rightFrames, QTransform().rotate(-90) },                  // 6: 上
            { &diagFrames,  QTransform::fromScale(1, -1) },              // 7: 左上
        };

        for (int d = 0; d < 8; ++d) {
            if (!dirInfos[d].source) continue;
            for (const QPixmap &src : *dirInfos[d].source) {
                if (dirInfos[d].transform.isIdentity()) {
                    g_projectileFrames[d].append(src);
                } else {
                    g_projectileFrames[d].append(
                        src.transformed(dirInfos[d].transform, Qt::SmoothTransformation));
                }
            }
        }
    }

    inline const QVector<QPixmap>& framesForDir(int dir)
    {
        static QVector<QPixmap> empty;
        if (dir < 0 || dir >= g_projectileFrames.size()) return empty;
        return g_projectileFrames[dir];
    }
}

void preloadProjectileFrames()
{
    doLoadProjectileFrames();
}

// ============================================================================
//  Projectile 火炮投射物（一技能）
// ============================================================================

Projectile::Projectile(QPointF startPos, QPointF velocity, int damage,
                       TileMap *tileMap, QGraphicsScene *scene,
                       QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      distanceTraveled(0.0),
      maxDistance(400.0),
      dirIndex(0),
      frameIndex(0),
      frameTick(0)
{
    // 预加载帧缓存（首次构造时执行一次）
    doLoadProjectileFrames();

    // 根据速度方向计算 8 方向索引
    qreal angle = qAtan2(velocity.y(), velocity.x()) * 180.0 / M_PI;
    if (angle < 0) angle += 360;
    dirIndex = qRound(angle / 45.0) % 8;

    // 设置第一帧
    const QVector<QPixmap> &frames = framesForDir(dirIndex);
    if (!frames.isEmpty()) {
        setPixmap(frames[0]);
    }

    setPos(startPos);
    setTransformationMode(Qt::SmoothTransformation);

    // 缩放到 40x40 显示
    QSize origSize = pixmap().size();
    if (!origSize.isEmpty()) {
        qreal sx = 40.0 / origSize.width();
        qreal sy = 40.0 / origSize.height();
        setTransform(QTransform::fromScale(sx, sy));
    }

    if (scene) {
        scene->addItem(this);
    }
}

Projectile::~Projectile()
{
    // 不再持有 QMovie，无需清理
}

bool Projectile::update()
{
    // ---- 移动 ----
    QPointF oldPos = pos();
    setPos(oldPos + velocity);
    distanceTraveled += QLineF(oldPos, pos()).length();

    // ---- 动画帧切换（每 2 个游戏帧切一帧，约 30fps）----
    frameTick++;
    if (frameTick >= 2) {
        frameTick = 0;
        const QVector<QPixmap> &frames = framesForDir(dirIndex);
        if (!frames.isEmpty()) {
            frameIndex = (frameIndex + 1) % frames.size();
            setPixmap(frames[frameIndex]);
        }
    }

    // ---- 生命周期检测 ----
    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  简易投射物（1-2 级红色椭圆子弹）
// ============================================================================

SimpleProjectile::SimpleProjectile(QPointF startPos, QPointF velocity, int damage,
                                   TileMap *tileMap, QGraphicsScene *scene,
                                   QGraphicsItem *parent,
                                   const QColor &color, int radius)
    : QGraphicsEllipseItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      distanceTraveled(0.0),
      maxDistance(400.0)
{
    setRect(-radius, -radius, radius * 2, radius * 2);
    setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), 220)));
    setPen(QPen(QColor(qMin(color.red() + 80, 255), qMin(color.green() + 80, 255), qMin(color.blue() + 80, 255), 220), 2));
    setPos(startPos);
    if (scene) scene->addItem(this);
}

SimpleProjectile::~SimpleProjectile()
{
}

bool SimpleProjectile::update()
{
    QPointF oldPos = pos();
    setPos(oldPos + velocity);
    distanceTraveled += QLineF(oldPos, pos()).length();

    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  三角形投射物（H键：单方向子弹）
// ============================================================================

TriangleProjectile::TriangleProjectile(QPointF startPos, QPointF velocity, int damage,
                                       TileMap *tileMap, QGraphicsScene *scene,
                                       QGraphicsItem *parent)
    : QGraphicsPolygonItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      distanceTraveled(0.0),
      maxDistance(400.0)
{
    QPolygonF triangle;
    triangle << QPointF(14, 0) << QPointF(-7, -10) << QPointF(-7, 10);
    setPolygon(triangle);
    setBrush(QBrush(QColor(255, 200, 0, 220)));
    setPen(QPen(QColor(255, 240, 100), 2));
    setPos(startPos);

    qreal angle = qAtan2(velocity.y(), velocity.x()) * 180.0 / M_PI;
    setRotation(angle);

    if (scene) scene->addItem(this);
}

TriangleProjectile::~TriangleProjectile()
{
}

bool TriangleProjectile::update()
{
    QPointF oldPos = pos();
    setPos(oldPos + velocity);
    distanceTraveled += QLineF(oldPos, pos()).length();

    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  冰魄八荒投射物（N键普攻2）
// ============================================================================

BlueProjectile::BlueProjectile(QPointF startPos, QPointF velocity, int damage,
                               TileMap *tileMap, QGraphicsScene *scene,
                               QGraphicsItem *parent)
    : QGraphicsPathItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      distanceTraveled(0.0),
      maxDistance(400.0)
{
    // 月牙形状：大圆减去向左偏移的小圆，默认朝右开口
    QPainterPath big;
    big.addEllipse(-12, -12, 24, 24);
    QPainterPath small;
    small.addEllipse(-16, -9, 18, 18); // 圆心(-4,0)，半径9，向左偏移
    QPainterPath crescent = big.subtracted(small);
    setPath(crescent);

    setBrush(QBrush(QColor(0, 140, 255, 220)));
    setPen(QPen(QColor(100, 200, 255, 220), 2));
    setPos(startPos);

    // 根据速度方向旋转，使月牙始终朝飞行方向
    qreal angle = qAtan2(velocity.y(), velocity.x()) * 180.0 / M_PI;
    setRotation(angle);

    if (scene) scene->addItem(this);
}

BlueProjectile::~BlueProjectile()
{
}

bool BlueProjectile::update()
{
    QPointF oldPos = pos();
    setPos(oldPos + velocity);
    distanceTraveled += QLineF(oldPos, pos()).length();

    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  二技能：刀浪（Blade Wave）
// ============================================================================

BladeWave::BladeWave(QPointF startPos, QPointF velocity, int damage,
                     TileMap *tileMap, QGraphicsScene *scene,
                     QGraphicsItem *parent)
    : QGraphicsRectItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      maxDistance(400),
      distanceTraveled(0.0)
{
    qreal vx = velocity.x();
    qreal vy = velocity.y();
    if (qAbs(vx) >= qAbs(vy)) {
        if (vx >= 0) {
            setRect(0, -8, 80, 16);
        } else {
            setRect(-80, -8, 80, 16);
        }
    } else {
        if (vy >= 0) {
            setRect(-8, 0, 16, 80);
        } else {
            setRect(-8, -80, 16, 80);
        }
    }
    setPos(startPos);

    setBrush(QBrush(QColor(0, 200, 255, 200)));
    setPen(QPen(QColor(200, 240, 255, 230), 2));

    if (scene) {
        scene->addItem(this);
    }
}

BladeWave::~BladeWave()
{
}

bool BladeWave::update()
{
    QPointF oldPos = pos();
    setPos(oldPos + velocity);

    distanceTraveled += QLineF(oldPos, pos()).length();
    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    return true;
}

// ============================================================================
//  2级+ GIF刀浪（Daolang Wave）
// ============================================================================

extern QVector<QPixmap> g_daolangFrames;

DaolangWave::DaolangWave(QPointF startPos, QPointF velocity, int damage,
                         TileMap *tileMap, QGraphicsScene *scene,
                         QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      distanceTraveled(0.0),
      maxDistance(400.0),
      frameIdx(0),
      frameTick(0)
{
    setTransformationMode(Qt::SmoothTransformation);
    setPos(startPos);

    if (!g_daolangFrames.isEmpty()) {
        QPixmap frame = g_daolangFrames[0];
        setPixmap(frame);
        setOffset(-frame.width() / 2.0, -frame.height() / 2.0);
        setTransformOriginPoint(frame.width() / 2.0, frame.height() / 2.0);
    }

    // 八方向旋转：GIF默认朝左（180°），旋转到速度方向
    qreal velocityAngle = qAtan2(velocity.y(), velocity.x()) * 180.0 / M_PI;
    setRotation(velocityAngle - 180.0);

    if (scene) scene->addItem(this);
}

DaolangWave::~DaolangWave()
{
}

bool DaolangWave::update()
{
    QPointF oldPos = pos();
    setPos(oldPos + velocity);
    distanceTraveled += QLineF(oldPos, pos()).length();

    if (distanceTraveled >= maxDistance) {
        return false;
    }

    if (tileMap && tileMap->collidesWithWall(this)) {
        return false;
    }

    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false;
        }
    }

    // 手动切换GIF帧
    if (!g_daolangFrames.isEmpty()) {
        frameTick++;
        if (frameTick >= 2) {
            frameTick = 0;
            frameIdx = (frameIdx + 1) % g_daolangFrames.size();
            setPixmap(g_daolangFrames[frameIdx]);
        }
    }

    return true;
}
