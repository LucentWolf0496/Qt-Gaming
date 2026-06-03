#include "pet.h"
#include "tilemap.h"
#include <QGraphicsScene>
#include <QDebug>
#include <QTransform>

Pet::Pet(QGraphicsScene *scene, TileMap *map, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent), tileMap(map)
{
    idleMovie = new QMovie(":/images/pet.gif");
    runMovie  = new QMovie(":/images/pet_right_run.gif");

    if (idleMovie->isValid()) {
        movie = idleMovie;
        connect(movie, &QMovie::frameChanged, this, &Pet::onFrameChanged);
        movie->start();
        onFrameChanged(0);
    } else {
        qDebug() << "Failed to load pet.gif";
    }

    setTransformationMode(Qt::SmoothTransformation);
    setZValue(1);  // 在地形之上(Z≤0)，玩家之下(Z=2)
    if (scene) scene->addItem(this);
}

Pet::~Pet()
{
    if (idleMovie) { idleMovie->stop(); delete idleMovie; }
    if (runMovie)  { runMovie->stop();  delete runMovie;  }
}

void Pet::onFrameChanged(int frame)
{
    Q_UNUSED(frame);
    if (!movie) return;

    QPixmap framePixmap = movie->currentPixmap();
    if (framePixmap.isNull()) return;

    if (!facingRight) {
        framePixmap = framePixmap.transformed(QTransform::fromScale(-1, 1), Qt::SmoothTransformation);
    }

    setPixmap(framePixmap);

    // Idle 时放大（56），Run 时正常（40）
    QSize origSize = framePixmap.size();
    qreal scale = (currentState == Idle) ? 32.0 / origSize.height() : 40.0 / origSize.height();
    setTransform(QTransform::fromScale(scale, scale));
}

void Pet::switchState(State newState)
{
    if (currentState == newState) return;
    currentState = newState;

    QMovie *oldMovie = movie;
    QMovie *newMovie = (currentState == Run) ? runMovie : idleMovie;

    if (oldMovie && oldMovie != newMovie) {
        disconnect(oldMovie, &QMovie::frameChanged, this, &Pet::onFrameChanged);
        oldMovie->stop();
    }

    movie = newMovie;
    if (movie && movie->isValid()) {
        connect(movie, &QMovie::frameChanged, this, &Pet::onFrameChanged);
        movie->start();
        onFrameChanged(0);
    }
}

void Pet::update(const QPointF &ownerPos)
{
    qreal dist = QLineF(pos(), ownerPos).length();

    // ---- 超出 21 tiles：立刻重置 ----
    if (dist > RESET_DIST) {
        resetToOwner(ownerPos);
        return;
    }

    // ---- 超出 5 tiles：持续向主角靠近 ----
    if (dist > APPROACH_DIST) {
        if (currentState != Run) switchState(Run);
        QPointF dir = ownerPos - pos();
        velocity = dir / dist * SPEED_APPROACH;
        facingRight = (velocity.x() >= 0);
    } else {
        // ---- 在 5 tiles 内：Idle 静止 ----
        if (currentState != Idle) switchState(Idle);
        velocity = QPointF(0, 0);
        return;
    }

    // ---- 移动 + 局部 DFS 绕墙 ----
    QPointF oldPos = pos();
    QPointF bestPos = oldPos;
    qreal bestDist = dist;

    // 尝试顺序：主方向 → 只X → 只Y → 8方向扫描
    QPointF candidates[11];
    int n = 0;
    candidates[n++] = oldPos + velocity;                          // 主方向
    candidates[n++] = QPointF(oldPos.x() + velocity.x(), oldPos.y()); // 只X
    candidates[n++] = QPointF(oldPos.x(), oldPos.y() + velocity.y()); // 只Y

    for (int i = 0; i < 8; ++i) {
        qreal a = i * M_PI / 4.0;
        candidates[n++] = oldPos + QPointF(qCos(a) * SPEED_APPROACH, qSin(a) * SPEED_APPROACH);
    }

    for (int i = 0; i < n; ++i) {
        setPos(candidates[i]);
        if (!tileMap || !tileMap->collidesWithWall(this)) {
            qreal d = QLineF(candidates[i], ownerPos).length();
            if (d < bestDist) {
                bestDist = d;
                bestPos = candidates[i];
            }
        }
    }

    setPos(bestPos);
    facingRight = (bestPos.x() >= oldPos.x());
}

void Pet::resetToOwner(const QPointF &ownerPos)
{
    setPos(ownerPos);
    if (currentState != Idle) {
        switchState(Idle);
    }
    stateTimer = 0;
    velocity = QPointF(0, 0);
}
