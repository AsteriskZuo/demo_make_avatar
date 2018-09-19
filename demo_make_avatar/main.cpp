#include "demo_make_avatar.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	demo_make_avatar w;
	w.show();
	return a.exec();
}
