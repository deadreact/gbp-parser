#include <iostream>
#include "gbpparser.hpp"
#include "context.hpp"
#include <QTextCodec>
#include <QApplication>
#include "tabwidget.h"


int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    TabWidget w;
    w.show();

    return app.exec();
}
