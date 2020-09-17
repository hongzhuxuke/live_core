#include "DSDemo.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DSDemo w;
    w.show();
    return a.exec();
}
