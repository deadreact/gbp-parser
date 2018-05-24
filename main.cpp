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
//    w.setText(parser.globalContext()->content().toString());
    w.show();

//    std::cout << "\e[0m\n\n\n" << QTextCodec::codecForLocale()->fromUnicode(parser.globalContext()->children().at(53)->content()).toStdString() << std::endl;

    return app.exec();
}
