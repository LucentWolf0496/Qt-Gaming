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
    // 占位图：深红色方块带白色 "巢" 字
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QBrush(QColor(139, 0, 0, 200)));
    painter.setPen(QPen(Qt::darkRed, 2));
    painter.drawRoundedRect(2, 2, 36, 36, 6, 6);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "巢");
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
