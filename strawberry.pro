#-------------------------------------------------
#
# Project created by QtCreator 2018-01-10T09:09:02
#
#-------------------------------------------------

QT       += core gui
QT += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = strawberry
TEMPLATE = app

RC_ICONS = stawberry.ico
SOURCES += main.cpp\
        mainwindow.cpp \
    mytcpsever.cpp

HEADERS  += mainwindow.h \
    mytcpsever.h \
    IPMsg.h

FORMS    += mainwindow.ui
