#ifndef MAIN_CPP
#define MAIN_CPP

#include "../hpp/mainwindow.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#endif // MAIN_CPP
