QT += core gui widgets
TEMPLATE = app
CONFIG += c++17

HEADERS += $$PWD/gbpparser.hpp \
           $$PWD/context.hpp \
    tabwidget.h \
    codegen.hpp \
    contextmodel.hpp

SOURCES += $$PWD/gbpparser.cpp \
           $$PWD/main.cpp \
           $$PWD/context.cpp \
    tabwidget.cpp \
    codegen.cpp \
    contextmodel.cpp

FORMS += \
    tabwidget.ui

TARGET = parser
DESTDIR = $$PWD
