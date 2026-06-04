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
    // 时空漩涡：蓝色竖椭圆，外蓝 → 中黑 → 内核亮蓝
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 外层蓝色光晕
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor(30, 80, 200, 200)));
    painter.drawEllipse(4, 0, 32, 40);

    // 中层深色过渡
    painter.setBrush(QBrush(QColor(10, 25, 80, 210)));
    painter.drawEllipse(9, 5, 22, 30);

    // 内层黑色核心
    painter.setBrush(QBrush(QColor(0, 0, 0, 230)));
    painter.drawEllipse(13, 10, 14, 20);

    // 中心亮蓝光点
    painter.setBrush(QBrush(QColor(80, 160, 255, 200)));
    painter.drawEllipse(16, 14, 8, 12);
    painter.end();

    setPixmap(pixmap);
    setTransformationMode(Qt::SmoothTransformation);
    setPos(pos);
    setZValue(1);  // 与宠物同级，在玩家(Z=2)之下

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
