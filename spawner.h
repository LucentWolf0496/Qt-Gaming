#ifndef SPAWNER_H
#define SPAWNER_H

#include <QGraphicsPixmapItem>
#include <QObject>
#include <QPointF>

class TileMap;
class QGraphicsScene;
class Game;

/**
 * @brief Spawner 怪物巢穴
 *
 * 每隔一定时间生成一波怪物，每波怪物的血量按倍率递增。
 */
class Spawner : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    Spawner(TileMap *tileMap, QGraphicsScene *scene, QPointF pos, Game *game);
    ~Spawner();

    /** 每帧更新（检查是否该生成新怪物） */
    void update();

private:
    TileMap *tileMap;
    QGraphicsScene *m_scene;
    Game *game;

    int spawnCounter;     // 生成计时器（帧）
    int spawnInterval;    // 生成间隔（帧），默认 300 ≈ 5 秒
    int spawnCount;       // 每次生成数量
    int wave;             // 当前波次
    qreal hpMultiplier;   // 每波血量倍率

    void doSpawn();       // 执行生成
};

#endif // SPAWNER_H
