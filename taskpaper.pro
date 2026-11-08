QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = taskpaper
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/highlighter.h \
    src/outline.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/highlighter.cpp \
    src/outline.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
