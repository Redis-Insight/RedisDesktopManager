#include "demo.h"
#include <QApplication>

int main(int argc, char *argv[])
{	
	QApplication a(argc, argv);

	QApplication::setApplicationName("Redis Desktop Manager");
	QApplication::setApplicationVersion("0.6.1-dev");	
	QApplication::setOrganizationDomain("redisdesktop.com");

	MainWin w;
	w.show();
	return a.exec();
}

