#include "app/mailapp.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    mailApp w;
    w.show();
    return a.exec();
}
