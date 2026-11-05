QT += core testlib
CONFIG += c++17 testcase
TARGET = taskpaper_tests
TEMPLATE = app

INCLUDEPATH += ../src
HEADERS += ../src/outline.h
SOURCES += taskpaper_tests.cpp ../src/outline.cpp
