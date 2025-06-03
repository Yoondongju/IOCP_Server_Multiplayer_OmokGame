#include "ChatServer.h"
#include <string>
#include <iostream>

#pragma comment(lib, "C:\\Program Files\\MySQL\\MySQL Server 8.0\\lib\\libmysql.lib")

int main()
{
	ChatServer server;

	//소켓을 초기화
	server.Init(MAX_IO_WORKER_THREAD);

	//소켓과 서버 주소를 연결하고 등록 시킨다.
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

