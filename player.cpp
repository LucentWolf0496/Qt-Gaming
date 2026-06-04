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
    if (keepCastFrame) return;  // 攻击后保持最后一帧
    if (!movie) return;

    QPixmap framePixmap = movie->currentPixmap();
    if (framePixmap.isNull()) return;

    // 朝左时水平翻转
    if (!facingRight) {
        framePixmap = framePixmap.transformed(QTransform::fromScale(-1, 1));
    }

    setPixmap(framePixmap);

    // 根据等级调整显示大小：1级32×32，2级+64×64
    int targetSize = (isTiny || level <= 1) ? 32 : 64;
    QSize origSize = framePixmap.size();
    qreal sx = (qreal)targetSize / origSize.width();
    qreal sy = (qreal)targetSize / origSize.height();
    setTransform(QTransform::fromScale(sx, sy));
}

void Player::updateAnimationState(bool moving, bool right, int vDir)
{
    facingRight = right;
    vertDir = vDir;
    isRunning = moving;

    // 施法期间不切换动画
    if (isCasting) return;

    // 移动时清除攻击保持帧和偏移
    if (moving) { keepCastFrame = false; setOffset(0, 0); }

    QString targetPath;
    if (moving) {
        if (isEnhanced && vertDir == -1) {
            targetPath = ":/images/player_enhanced_back_run.gif";   // 上
        } else if (isEnhanced && vertDir == 1) {
            targetPath = ":/images/player_enhanced_front_run.gif";  // 下
        } else if (isEnhanced) {
            targetPath = ":/images/player_enhanced_right_run.gif";  // 水平
        } else {
            targetPath = ":/images/player_run.gif";
        }
    } else {
        targetPath = isEnhanced ? ":/images/player_enhanced.gif"
                                : ":/images/player.gif";
    }

    if (currentGifPath == targetPath) return;

    if (movie) {
        currentGifPath = targetPath;
        movie->stop();
        movie->setFileName(targetPath);
        movie->start();
    }
}

void Player::playCastAnimation(const QString &gifPath, int frameInterval, int displaySize, bool keepFrame)
{
    if (!isEnhanced || isCasting || !movie) return;

    keepCastFrame = false;  // 允许新动画开始
    castKeepFrame = keepFrame;
    castDisplaySize = displaySize;
    castFrames.clear();
    QImageReader reader(gifPath);
    reader.setAutoDetectImageFormat(true);
    while (reader.canRead()) {
        QImage img = reader.read();
        if (!img.isNull()) {
            castFrames.append(QPixmap::fromImage(img).scaled(
                displaySize, displaySize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    if (castFrames.isEmpty()) return;

    isCasting = true;
    castFrameIdx = 0;
    castFrameTick = 0;
    castFrameInterval = qMax(1, frameInterval);
    // 居中偏移：大图超出64的部分的一半
    int offset = (displaySize - 64) / 2;
    setOffset(-offset, -offset);
    movie->stop();
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
            // 播放完毕，根据参数决定是否保持帧
            isCasting = false;
            keepCastFrame = castKeepFrame;
            if (!castKeepFrame) { setOffset(0, 0); updateAnimationState(isRunning, facingRight, vertDir); }
            currentGifPath.clear();
        }
    }
}

void Player::setEnhanced(bool enhanced)
{
    if (isEnhanced == enhanced) return;
    isEnhanced = enhanced;

    // 强制刷新动画
    currentGifPath.clear();
    updateAnimationState(isRunning, facingRight, vertDir);

    qDebug() << "Player enhanced mode:" << (enhanced ? "ON" : "OFF");
}

void Player::move(bool up, bool down, bool left, bool right)
{
    qreal dx = 0, dy = 0;
    if (left)  dx = -speed;
    if (right) dx =  speed;
    if (up)    dy = -speed;
    if (down)  dy =  speed;

    // 更新朝向
    if (dx > 0)      { facingRight = true;  vertDir = 0; }
    else if (dx < 0) { facingRight = false; vertDir = 0; }
    else if (dy < 0) { vertDir = -1; }  // 上
    else if (dy > 0) { vertDir =  1; }  // 下

    bool moving = (dx != 0 || dy != 0);
    updateAnimationState(moving, facingRight, vertDir);

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
    if (hp == 0) {
        emit died();   // 发射死亡信号
    }
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

// 在 player.cpp 末尾或其他合适位置添加
void Player::reset()
{
    // 重置等级与经验
    level = 1;
    exp = 0;
    maxExp = 100;
    // 重置 HP/MP
    hp = 100;
    maxHp = 100;
    mp = 100;
    maxMp = 100;
    // 关闭增强形态
    setEnhanced(false);
    // 强制刷新动画（重新计算缩放和GIF）
    currentGifPath.clear();
    updateAnimationState(isRunning, facingRight, vertDir);
    // 重新计算显示大小（等级1是32x32）
    onFrameChanged(0);
    qDebug() << "Player reset to level 1, HP/MP full.";
}

void Player::setLevel(int lvl)
{
    if (lvl < 1) lvl = 1;
    else {}
    if (lvl > 20) lvl = 20;
    else {}
    level = lvl;
    maxExp = 100;
    for (int i = 1; i < lvl; i++) maxExp = static_cast<int>(maxExp * 1.2);
    exp = 0;
    maxHp = 100; maxMp = 100;
    for (int i = 1; i < lvl; i++) { maxHp = static_cast<int>(maxHp * 1.5); maxMp = static_cast<int>(maxMp * 1.5); }
    hp = maxHp; mp = maxMp;
    onFrameChanged(0);
    qDebug() << "Admin: set level" << lvl << "HP:" << hp << "MP:" << mp;
}

void Player::restoreState(int lvl, int e, int maxE, int h, int maxH, int m, int maxM, bool enhanced)
{
    level = lvl;
    exp = e;
    maxExp = maxE;
    hp = h;
    maxHp = maxH;
    mp = m;
    maxMp = maxM;
    setEnhanced(enhanced);  // 刷新动画和形态
    qDebug() << "Player state restored: Lv." << level << "HP:" << hp << "/" << maxHp;
}

void Player::clearCastState()
{
    keepCastFrame = false;
    castFrames.clear();
    setOffset(0, 0);
    updateAnimationState(isRunning, facingRight, vertDir);
}

void Player::setTiny(bool tiny)
{
    if (isTiny == tiny) return;
    isTiny = tiny;
    onFrameChanged(0);  // 刷新显示大小
    qDebug() << "Player tiny mode:" << (tiny ? "ON (1×1)" : "OFF (96×96)");
}