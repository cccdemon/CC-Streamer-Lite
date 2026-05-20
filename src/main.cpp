#include "app/Application.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication qtApp(argc, argv);

    ccstreamer::Application app;
    app.start();

    return QApplication::exec();
}

