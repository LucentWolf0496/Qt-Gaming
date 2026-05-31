#ifndef PET_H
#define PET_H

#include <QGraphicsPixmapItem>
#include <QObject>
#include <QMovie>
#include <QPointF>
#include <QRandomGenerator>
#include <QtMath>

class QGraphicsScene;
class TileMap;

/**
 * @brief Pet 宠物
 *
 * 跟随玩家的宠物，使用 GIF 动画。
 * - 超出 21×21（672px）时立刻重置到玩家位置
 * - 超出 8 格子（256px）时持续向玩家靠近（速度较慢）
 * - 在 8 格子内随机 Idle/Run 游荡
 * - 撞墙阻挡，不能穿墙
 * - ZValue 在玩家下方，不遮挡主角
 */
class Pet : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    Pet(QGraphicsScene *scene, TileMap *tileMap, QGraphicsItem *parent = nullptr);
    ~Pet();

    void update(const QPointF &ownerPos);
    void resetToOwner(const QPointF &ownerPos);

private slots:
    void onFrameChanged(int frame);

private:
    enum State { Idle, Run };
    void switchState(State newState);

    QMovie *idleMovie = nullptr;
    QMovie *runMovie  = nullptr;
    QMovie *movie     = nullptr;
    TileMap *tileMap  = nullptr;

    State currentState = Idle;
    int   stateTimer   = 0;
    QPointF velocity;
    bool    facingRight = true;

    static constexpr qreal SPEED_APPROACH = 3.5;   // 靠近速度
    static constexpr qreal RESET_DIST     = 672.0; // 21 tiles
    static constexpr qreal APPROACH_DIST  = 160.0; // 5 tiles
};

#endif // PET_H
