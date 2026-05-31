TEMPLATE = app
TARGET = Qt-Gaming
QT += widgets
RESOURCES += resources.qrc

SOURCES += \
    main.cpp \
    game.cpp \
    player.cpp \
    tilemap.cpp \
    tile.cpp \
    maploader.cpp \
    skill.cpp \
    enemy.cpp \
    spawner.cpp \
    pet.cpp

HEADERS += \
    game.h \
    player.h \
    tilemap.h \
    tile.h \
    maploader.h \
    skill.h \
    enemy.h \
    spawner.h \
    pet.h

CONFIG += console