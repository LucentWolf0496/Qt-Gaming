#ifndef PLAYER_H
#define PLAYER_H

#include <QGraphicsPixmapItem>
#include <QObject>
#include <QMovie>

class TileMap;

class Player : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit Player(TileMap *map, QGraphicsItem *parent = nullptr);
    ~Player();

    void move(bool up, bool down, bool left, bool right);
    QRectF hitboxRect() const {
        if (isTiny || level <= 1)
            return QRectF(x(), y(), 32, 32);
        // 64×64 = 2×2格，右下角：(+32, +32)
        return QRectF(x() + 32, y() + 32, 32, 32);
    }
    int getDisplaySize() const { return (level <= 1) ? 32 : 64; }
    void setTiny(bool tiny);
    bool isBusy() const { return isCasting || keepCastFrame; }
    bool isCastingNow() const { return isCasting; }  // 仅检查是否在播放帧
    void clearCastState();   // 强制清除攻击动画状态，恢复站立

    // ========== 血量蓝量系统 ==========
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getMp() const { return mp; }
    int getMaxMp() const { return maxMp; }

    void takeDamage(int dmg);
    bool consumeMp(int cost);
    void recoverHpMp(int hpAmount, int mpAmount);

    // ========== 经验等级系统 ==========
    void addExp(int amount);
    int getExp() const { return exp; }
    int getMaxExp() const { return maxExp; }
    int getLevel() const { return level; }
    void setSpeed(qreal s) { speed = s; }
    qreal getSpeed() const { return speed; }
    void setLevel(int lvl);  // 管理员直接设级

    // ========== 形态切换 ==========
    void setEnhanced(bool enhanced);
    bool getEnhanced() const { return isEnhanced; }
    void playCastAnimation(const QString &gifPath, int frameInterval = 2, int displaySize = 64, bool keepFrame = false);
    void updateCastAnimation();

    // ========== 死亡重置 ==========
    void reset();          // 新增：重置玩家到初始状态

    // ========== 跨地图状态保持 ==========
    void restoreState(int lvl, int e, int maxE, int h, int maxH, int m, int maxM, bool enhanced);

signals:
    void levelUp(int newLevel);
    void died();           // 新增：死亡信号

private slots:
    void onFrameChanged(int frame);

private:
    TileMap *tileMap;
    qreal speed;
    QMovie *movie;

    int hp = 100;
    int maxHp = 100;
    int mp = 100;
    int maxMp = 100;

    int exp = 0;
    int maxExp = 100;
    int level = 1;

    bool facingRight = true;
    int  vertDir = 0;      // -1=上, 0=水平, 1=下
    bool isTiny = false;   // 传送过渡临时缩为1×1(32×32)
    bool isRunning = false;
    bool isEnhanced = false;
    bool isCasting = false;
    bool keepCastFrame = false;  // 攻击后保持最后一帧，直到移动
    QVector<QPixmap> castFrames;
    int castFrameIdx = 0;
    int castDisplaySize = 96;  // 攻击动画显示尺寸（bright版=115）
    bool castKeepFrame = false; // 动画结束后是否保持最后一帧
    int castFrameTick = 0;
    int castFrameInterval = 2;
    QString currentGifPath;

    void updateAnimationState(bool moving, bool right, int vDir = 0);
};

#endif // PLAYER_H