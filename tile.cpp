#include "tile.h"
#include <QPixmap>
#include <QDebug>
#include <QTransform>

Tile::Tile(const QString &imagePath, qreal x, qreal y,
           const QSize &fixedSize, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        qDebug() << "Failed to load tile image:" << imagePath;
    } else {
        setPixmap(pixmap);
        setTransformationMode(Qt::SmoothTransformation);
        // 保留原始分辨率，用 transform 缩放到目标显示大小
        if (fixedSize.isValid()) {
            qreal sx = static_cast<qreal>(fixedSize.width())  / pixmap.width();
            qreal sy = static_cast<qreal>(fixedSize.height()) / pixmap.height();
            setTransform(QTransform::fromScale(sx, sy));
        }
    }
    setPos(x, y);
}
