QT       += core gui widgets serialport

CONFIG   += c++14 console
CONFIG   -= app_bundle

TARGET   = uart_host_simulator
TEMPLATE = app

SOURCES  += main.cpp \
            mainwindow.cpp

HEADERS  += mainwindow.h \
            protocol.hpp

# Qt Creator 4.11 + Qt 5.14.1 (MSVC 2017 32-bit) 可直接打开本 .pro 编译
