#include "ChatServer.h"
#include <string>
#include <iostream>

#pragma comment(lib, "C:\\Program Files\\MySQL\\MySQL Server 8.0\\lib\\libmysql.lib")

int main()
{
	ChatServer server;

	//������ �ʱ�ȭ
	server.Init(MAX_IO_WORKER_THREAD);

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);


	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	

	return 0;
}

