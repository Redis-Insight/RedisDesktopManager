#include <QTest>
#include <QApplication>

//tests
#include <iostream>
#include "test_redisconnection.h"
#include "test_redisconnectionsmanager.h"
#include "test_command.h"
#include "test_response.h"
#include "test_valueformatters.h"

int main(int argc, char *argv[])
{
	QApplication app( argc, argv );

	int allTestsResult = 
		QTest::qExec(new TestCommand, argc, argv) +
		QTest::qExec(new TestResponse, argc, argv) +
		QTest::qExec(new TestRedisConnection, argc, argv) +		
		QTest::qExec(new TestRedisConnectionsManager, argc, argv) +
		QTest::qExec(new TestValueFormatters, argc, argv);

    if (allTestsResult != 0 ) {

		#ifdef WIN32		
		std::cin.get();		
		#endif // WIN32

        return 1;
    }

	return 0;
}

