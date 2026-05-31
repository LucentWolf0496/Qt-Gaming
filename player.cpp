#include "player.h"
#include "tilemap.h"
#include <QPixmap>
#include <QDebug>
#include <QMovie>
#include <QTransform>
#include <QImageReader>

Player::Player(TileMap *map, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent), tileMap(map), speed(4.0), movie(nullptr)
{
    setZValue(2); // 确保画在火焰背景(Z=1)和地板(Z=0)之上
    // 加载 idle GIF
    movie = new QMovie(":/images/player.gif");
    if (movie->isValid()) {
        connect(movie, &QMovie::frameChanged, this, &Player::onFrameChanged);
        movie->start();
        currentGifPath = ":/images/player.gif";
    } else {
        qDebug() << "Failed to load player.gif, falling back to static image.";
        delete movie;
        movie = nullptr;

        QPixmap pixmap(":/images/player.png");
        if (pixmap.isNull()) {
            qDebug() << "Failed to load player.png, creating blue placeholder.";
            pixmap = QPixmap(32, 32);
            pixmap.fill(Qt::blue);
        }
        setPixmap(pixmap);
        QSize origSize = pixmap.size();
        qreal sx = 64.0 / origSize.width();
        qreal sy = 64.0 / origSize.height();
        setTransform(QTransform::fromScale(sx, sy));
    }
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    setTransformationMode(Qt::SmoothTransformation);
}

Player::~Player()
{
    if (movie) {
        movie->stop();
        delete movie;
        movie = nullptr;
    }
}

void Player::onFrameChanged(int frame)
{
    Q_UNUSED(frame);
    if (!movie) return;

    QPixmap framePixmap = movie->currentPixmap();
    if (framePixmap.isNull()) return;

    // 朝左时水平翻转
    if (!facingRight) {
        framePixmap = framePixmap.transformed(QTransform::fromScale(-1, 1));
    }

    setPixmap(framePixmap);

    // 根据等级调整显示大小：1级32×32，2级+64×64
    int targetSize = (level <= 1) ? 32 : 64;
    QSize origSize = framePixmap.size();
    qreal sx = (qreal)targetSize / origSize.width();
    qreal sy = (qreal)targetSize / origSize.height();
    setTransform(QTransform::fromScale(sx, sy));
}

void Player::updateAnimationState(bool moving, bool right)
{
    facingRight = right;
    isRunning = moving;

    // 施法期间不切换动画（但 isRunning 已更新，施法结束时会正确恢复）
    if (isCasting) return;

    // 如果移动状态没变且朝向没变，无需切换
    if (isRunning == moving && facingRight == right && !currentGifPath.isEmpty()) {
        // 但形态可能变了，需要检查
        QString expectedPath;
        if (moving) {
            expectedPath = isEnhanced ? ":/images/player_enhanced_right_run.gif"
                                      : ":/images/player_run.gif";
        } else {
            expectedPath = isEnhanced ? ":/images/player_enhanced.gif"
                                      : ":/images/player.gif";
        }
        if (currentGifPath == expectedPath) return;
    }

    QString targetPath;
    if (moving) {
        targetPath = isEnhanced ? ":/images/player_enhanced_right_run.gif"
                                : ":/images/player_run.gif";
    } else {
        targetPath = isEnhanced ? ":/images/player_enhanced.gif"
                                : ":/images/player.gif";
    }

    if (currentGifPath != targetPath && movie) {
        currentGifPath = targetPath;
        movie->stop();
        movie->setFileName(targetPath);
        movie->start();
    }
}

void Player::playCastAnimation(const QString &gifPath, int frameInterval)
{
    if (!isEnhanced || isCasting || !movie) return;

    // 预加载所有帧到内存
    castFrames.clear();
    QImageReader reader(gifPath);
    reader.setAutoDetectImageFormat(true);
    while (reader.canRead()) {
        QImage img = reader.read();
        if (!img.isNull()) {
            castFrames.append(QPixmap::fromImage(img).scaled(
                64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    if (castFrames.isEmpty()) return;

    isCasting = true;
    castFrameIdx = 0;
    castFrameTick = 0;
    castFrameInterval = qMax(1, frameInterval);
    movie->stop(); // 暂停正常动画
}

void Player::updateCastAnimation()
{
    if (!isCasting || castFrames.isEmpty()) return;

    // 每帧都更新显示（处理转身翻转）
    QPixmap frame = castFrames[castFrameIdx];
    if (!facingRight) {
        frame = frame.transformed(QTransform::fromScale(-1, 1), Qt::SmoothTransformation);
    }
    setPixmap(frame);
    setTransform(QTransform()); // 帧已缩放到 64x64，无需额外变换

    castFrameTick++;
    if (castFrameTick >= castFrameInterval) {
        castFrameTick = 0;
        castFrameIdx++;
        if (castFrameIdx >= castFrames.size()) {
            // 播放完毕，恢复原有动画
            isCasting = false;
            castFrames.clear();
            currentGifPath.clear();
            updateAnimationState(isRunning, facingRight);
        }
    }
}

void Player::setEnhanced(bool enhanced)
{
    if (isEnhanced == enhanced) return;
    isEnhanced = enhanced;

    // 强制刷新动画
    currentGifPath.clear();
    updateAnimationState(isRunning, facingRight);

    qDebug() << "Player enhanced mode:" << (enhanced ? "ON" : "OFF");
}

void Player::move(bool up, bool down, bool left, bool right)
{
    qreal dx = 0, dy = 0;
    if (left)  dx = -speed;
    if (right) dx =  speed;
    if (up)    dy = -speed;
    if (down)  dy =  speed;

    // 更新朝向（只有水平移动才改变朝向，静止时保持原朝向）
    if (dx > 0) facingRight = true;
    else if (dx < 0) facingRight = false;

    bool moving = (dx != 0 || dy != 0);
    updateAnimationState(moving, facingRight);

    if (dx == 0 && dy == 0) return;

    setPos(x() + dx, y());
    if (tileMap->collidesWithWall(hitboxRect()))
        setPos(x() - dx, y());

    setPos(x(), y() + dy);
    if (tileMap->collidesWithWall(hitboxRect()))
        setPos(x(), y() - dy);
}

void Player::takeDamage(int dmg)
{
    hp -= dmg;
    if (hp < 0) hp = 0;
    qDebug() << "Player took damage:" << dmg << "HP:" << hp << "/" << maxHp;
}

bool Player::consumeMp(int cost)
{
    if (mp < cost) {
        qDebug() << "Not enough MP! Need" << cost << "have" << mp;
        return false;
    }
    mp -= cost;
    qDebug() << "Consumed MP:" << cost << "MP left:" << mp << "/" << maxMp;
    return true;
}

void Player::recoverHpMp(int hpAmount, int mpAmount)
{
    hp += hpAmount;
    if (hp > maxHp) hp = maxHp;
    mp += mpAmount;
    if (mp > maxMp) mp = maxMp;
}

void Player::addExp(int amount)
{
    if (level >= 20) return; // 最高 20 级
    exp += amount;
    while (exp >= maxExp && level < 20) {
        exp -= maxExp;
        level++;
        maxExp = static_cast<int>(maxExp * 1.2);
        // 升级回满 HP/MP，且上限提升 1.5 倍
        maxHp = static_cast<int>(maxHp * 1.5);
        maxMp = static_cast<int>(maxMp * 1.5);
        hp = maxHp;
        mp = maxMp;
        emit levelUp(level);
        onFrameChanged(0); // 强制刷新显示大小
        qDebug() << "Level up! New level:" << level
                 << "HP:" << hp << "/" << maxHp
                 << "MP:" << mp << "/" << maxMp
                 << "Next EXP:" << maxExp;
    }
}
