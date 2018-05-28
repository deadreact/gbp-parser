QT += core gui widgets
TEMPLATE = app
CONFIG += c++17

HEADERS += $$PWD/gbpparser.hpp \
           $$PWD/context.hpp \
    tabwidget.h \
    codegen.hpp \
    contextmodel.hpp \
    page.h \
    checkedfileslist.hpp

SOURCES += $$PWD/gbpparser.cpp \
           $$PWD/main.cpp \
           $$PWD/context.cpp \
    tabwidget.cpp \
    codegen.cpp \
    contextmodel.cpp \
    page.cpp \
    checkedfileslist.cpp

FORMS += \
    page.ui \
    tabwidget.ui \
    checkedfileslist.ui

TARGET = parser
DESTDIR = $$PWD
