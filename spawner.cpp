#include "spawner.h"
#include "enemy.h"
#include "game.h"
#include <QGraphicsScene>
#include <QRandomGenerator>
#include <QPixmap>
#include <QPainter>
#include <QDebug>

Spawner::Spawner(TileMap *tileMap, QGraphicsScene *scene, QPointF pos, Game *game)
    : QGraphicsPixmapItem(),
      tileMap(tileMap),
      m_scene(scene),
      game(game),
      spawnCounter(0),
      spawnInterval(600), // 10 秒（60fps）
      spawnCount(1),      // 每次生成 1 只
      wave(0),
      hpMultiplier(1.2)
{
    // 时空漩涡：3×3 tiles (96×96)，中心定位
    const int SZ = 96;
    QPixmap pixmap(SZ, SZ);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    // 外层蓝色光晕
    painter.setBrush(QBrush(QColor(30, 80, 200, 200)));
    painter.drawEllipse(10, 0, 76, 96);
    // 中层深色过渡
    painter.setBrush(QBrush(QColor(10, 25, 80, 210)));
    painter.drawEllipse(22, 12, 52, 72);
    // 内层黑色核心
    painter.setBrush(QBrush(QColor(0, 0, 0, 230)));
    painter.drawEllipse(31, 24, 34, 48);
    // 中心亮蓝光点
    painter.setBrush(QBrush(QColor(80, 160, 255, 200)));
    painter.drawEllipse(38, 34, 20, 28);
    painter.end();

    setPixmap(pixmap);
    setTransformationMode(Qt::SmoothTransformation);
    setPos(pos.x() - SZ/2, pos.y() - SZ/2);  // 中心定位
    setZValue(4);  // 与传送门同级

    if (scene) {
        scene->addItem(this);
    }
}

Spawner::~Spawner()
{
}

void Spawner::update()
{
    spawnCounter++;
    if (spawnCounter >= spawnInterval) {
        spawnCounter = 0;
        doSpawn();
    }
}

void Spawner::doSpawn()
{
    if (!game || !m_scene) return;

    // 限制场上怪物数量，超过阈值不再生成
    const int MAX_ENEMIES = 10;
    if (game->getEnemyCount() >= MAX_ENEMIES) {
        return;
    }

    int baseHp = 50;
    int hp = static_cast<int>(baseHp * qPow(hpMultiplier, wave));

    for (int i = 0; i < spawnCount; ++i) {
        qreal offsetX = QRandomGenerator::global()->bounded(40) - 20;
        qreal offsetY = QRandomGenerator::global()->bounded(40) - 20;
        QPointF spawnPos = pos() + QPointF(offsetX, offsetY);

        Enemy *e = new Enemy(tileMap, m_scene, spawnPos, game, hp);
        game->addEnemy(e);
    }

    wave++;
    qDebug() << "Spawner spawned" << spawnCount << "enemies with HP" << hp << "(wave" << wave << ")";
}
