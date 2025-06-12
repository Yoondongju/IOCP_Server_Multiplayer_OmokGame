#include <utility>
#include <cstring>


#include "MyDBManager.h"

#include "UserManager.h"
#include "RoomManager.h"
#include "PacketManager.h"
#include "RedisManager.h"
#include "GameManager.h"


void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;


	mRecvFuntionDictionary[(int)PACKET_ID::REGISTER_REQUEST] = &PacketManager::ProcessRegister;

	mRecvFuntionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;
	
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
				

	mRecvFuntionDictionary[(int)PACKET_ID::MY_DATA_REQUEST] = &PacketManager::ProcessUserData;
	mRecvFuntionDictionary[(int)PACKET_ID::OTHER_USER_DATA_REQUEST] = &PacketManager::ProcessOtherUserData;
	

	mRecvFuntionDictionary[(int)PACKET_ID::START_GAME_REQUEST_PACKET] = &PacketManager::ProcessStartGame;
	mRecvFuntionDictionary[(int)PACKET_ID::PUT_STONE_REQUEST_PACKET] = &PacketManager::ProcessStoneLogic;


	CreateCompent(maxClient_);

	mRedisMgr = new RedisManager;// std::make_unique<RedisManager>();
	mDBMgr = new MyDBManager;
	if (!mDBMgr->IsConnected())
		mDBMgr->Connect();

	mGameMgr = new GameManager;
}

void PacketManager::CreateCompent(const UINT32 maxClient_)
{
	mUserManager = new UserManager;
	mUserManager->Init(maxClient_);

		
	UINT32 startRoomNummber = 0;	// �� ��ȣ
	UINT32 maxRoomCount = 10;		// �� �ִ� ����
	UINT32 maxRoomUserCount = 4;	// �� �ִ� ���� �ο�
	mRoomManager = new RoomManager;
	mRoomManager->SendPacketFunc = SendPacketFunc;
	mRoomManager->Init(startRoomNummber, maxRoomCount, maxRoomUserCount);
}

bool PacketManager::Run()
{	
	if (mRedisMgr->Run("127.0.0.1", 6379, 1) == false)	// Redis�ʱ�ȭ�� Redisó�� ������ ����
	{
		return false;
	}

	//�� �κ��� ��Ŷ ó�� �κ����� �̵� ��Ų��.
	mIsRunProcessThread = true;
	mProcessThread = std::thread([this]() { ProcessPacket(); });

	printf("��Ŷó�� ������ ����..\n");

	return true;
}

void PacketManager::End()
{
	mGameMgr->Free();

	mRedisMgr->End();
	mDBMgr->Disconnect();

	mIsRunProcessThread = false;

	if (mProcessThread.joinable())
	{
		mProcessThread.join();
	}
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pReqUser->GetDomainState() == User::DOMAIN_STATE::ROOM)
	{
		auto roomNum = pReqUser->GetCurrentRoom();
		mRoomManager->LeaveUser(roomNum, pReqUser);
	}

	if (pReqUser->GetDomainState() != User::DOMAIN_STATE::NONE)
	{
		mUserManager->DeleteUserInfo(pReqUser);
	}
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	EnqueuePacketData(clientIndex_);
}

void PacketManager::EnqueuePacketData(const UINT32 clientIndex_)
{
	std::lock_guard<std::mutex> guard(mLock);
	mInComingPacketUserIndex.push_back(clientIndex_);
}

PacketInfo PacketManager::DequePacketData()
{
	UINT32 userIndex = 0;

	{
		std::lock_guard<std::mutex> guard(mLock);
		if (mInComingPacketUserIndex.empty())
		{
			return PacketInfo();
		}

		userIndex = mInComingPacketUserIndex.front();	// �̰� ��Ŷ���� Ŭ���� �ε�����!
		mInComingPacketUserIndex.pop_front();
	}

	auto pUser = mUserManager->GetUserByConnIdx(userIndex);
	auto packetData = pUser->GetPacket();	// ������ ����մ� ��Ŷ�� �����´�
	packetData.ClientIndex = userIndex;
	return packetData;
}

void PacketManager::PushSystemPacket(PacketInfo packet_)
{
	std::lock_guard<std::mutex> guard(mLock);
	mSystemPacketQueue.push_back(packet_);
}

PacketInfo PacketManager::DequeSystemPacketData()
{

	std::lock_guard<std::mutex> guard(mLock);
	if (mSystemPacketQueue.empty())
	{
		return PacketInfo();
	}

	auto packetData = mSystemPacketQueue.front();
	mSystemPacketQueue.pop_front();

	return packetData;
}


void PacketManager::ProcessPacket()
{
	while (mIsRunProcessThread)
	{
		bool isIdle = true;

		if (auto packetData = DequePacketData(); packetData.PacketId > (UINT16)PACKET_ID::SYS_END)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (auto packetData = DequeSystemPacketData(); packetData.PacketId != 0)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (auto task = mRedisMgr->TakeResponseTask(); task.TaskID != RedisTaskID::INVALID)
		{
			isIdle = false;
			ProcessRecvPacket(task.UserIndex, (UINT16)task.TaskID, task.DataSize, task.pData);
			task.Release();
		}

		if(isIdle)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_)
{
	auto iter = mRecvFuntionDictionary.find(packetId_);
	if (iter != mRecvFuntionDictionary.end())
	{
		(this->*(iter->second))(clientIndex_, packetSize_, pPacket_);
	}

}

void PacketManager::ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("[ProcessUserConnect] clientIndex: %d\n", clientIndex_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->Clear();
}

void PacketManager::ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("[ProcessUserDisConnect] clientIndex: %d\n", clientIndex_);
	ClearConnectionInfo(clientIndex_);
}

void PacketManager::ProcessRegister(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	REGISTER_REQUEST_PACKET* pRegisterPacket = (REGISTER_REQUEST_PACKET*)pPacket_;
	REGISTER_RESPONSE_PACKET resPacket{};
	resPacket.PacketLength = sizeof(REGISTER_RESPONSE_PACKET);

	if (IsInvalidUserID(pRegisterPacket->UserID))
	{		
		resPacket.PacketId = (UINT16)PACKET_ID::REGISTER_RESPONSE;
		resPacket.Result = (UINT16)ERROR_CODE::USER_MGR_INVALID_USER_UNIQUEID;
		SendPacketFunc(clientIndex_, sizeof(REGISTER_RESPONSE_PACKET), (char*)&resPacket);
		return;
	}


	char query[512];
	snprintf(query, sizeof(query),
		"INSERT INTO users (user_id, password_hash) VALUES ('%s', '%s')",		// DB�� ����
		pRegisterPacket->UserID, pRegisterPacket->UserPW);


	MYSQL* const sqlconn = mDBMgr->Get_SqlConn();
	int queryResult = mysql_query(sqlconn, query);

	
	resPacket.PacketId = (UINT16)PACKET_ID::REGISTER_RESPONSE;

	if (queryResult != 0)
	{
		// ���� �α�
		std::cout << "MySQL ���� ����: " << mysql_error(sqlconn) << "\n";

		// �ߺ� ID�� ��� (Error code 1062) ���� ����
		if (mysql_errno(sqlconn) == 1062)
		{
			resPacket.Result = 2; // ID �ߺ� ���� �ڵ� ����
		}
		else
		{
			resPacket.Result = 1; // �Ϲ� ����
		}
	}
	else
	{
		char statQuery[256];
		snprintf(statQuery, sizeof(statQuery),
			"INSERT INTO user_stats (user_id, total_matches, wins, losses) VALUES ('%s', 0, 0, 0)",
			pRegisterPacket->UserID);

		if (mysql_query(sqlconn, statQuery) != 0)
		{
			std::cout << "���� �ʱ�ȭ ���� ����: " << mysql_error(sqlconn) << "\n";
		}

		resPacket.Result = 0; // ����
	}

	SendPacketFunc(clientIndex_, sizeof(REGISTER_RESPONSE_PACKET), (char*)&resPacket);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{ 
	if (LOGIN_REQUEST_PACKET_SZIE != packetSize_)
	{
		printf("�߸��� ũ���� ��Ŷ�������Դϴ�.\n");
		return;
	}

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);

	auto pUserID = pLoginReqPacket->UserID;
	printf("�α��� ��û�� �������̵� >> %s\n", pUserID);

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	
	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt()) 
	{ 
		//�����ڼ��� �ִ���� �����ؼ� ���ӺҰ�
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET) , (char*)&loginResPacket);
		return;
	}

	//���⿡�� �̹� ���ӵ� �������� Ȯ���ϰ�, ���ӵ� ������� �����Ѵ�.
	if (mUserManager->FindUserIndexByID(pUserID) == -1)		// ����
	{ 
		MYSQL* const sqlconn = mDBMgr->Get_SqlConn();
		printf("MySql���� ���̵� ã����.. >> %s\n", pUserID);

		char query[512];
		snprintf(query, sizeof(query),
			"SELECT password_hash FROM users WHERE user_id='%s' LIMIT 1;",
			pUserID);

		if (mysql_query(sqlconn, query) != 0)  // ���� ���н�
		{
			printf("MySQL ���� ����: %s\n", mysql_error(sqlconn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
			return;
		}

		MYSQL_RES* result = mysql_store_result(sqlconn);
		if (!result)
		{
			printf("MySQL ��� ���� �Ǵ� ����: %s\n", mysql_error(sqlconn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(result);
		if (row == nullptr)
		{
			// ���̵� ����
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

			mysql_free_result(result);
			return;
		}

		const char* storedHash = row[0];
		mysql_free_result(result);

		// ���⼭ ��й�ȣ ����
		// ���� ���񽺶�� bcrypt ���� �ؽ� ���� �ؾ� ������, ������ �ܼ� �� ����
		if (strcmp(storedHash, pLoginReqPacket->UserPW) == 0)
		{
			// �α��� ����
			loginResPacket.Result = (UINT16)ERROR_CODE::NONE;
			mUserManager->AddUser(pLoginReqPacket->UserID, clientIndex_);

			User* pUser = mUserManager->GetUserByConnIdx(clientIndex_);
			const USER_DATA& Data = pUser->Get_MyData();

			if (false == LoadUserStat(pUser))
			{
				printf("���� ���� �ҷ����� ����: %s\n", pUserID);
			}
			else
			{
				printf("���� ���� �ҷ����� ����: %s (��:%d ��:%d ��:%d)\n",
					pUserID, Data.iWin, Data.iLose, Data.iTotalMatch);

				USER_DATA_PACKET UserDataPacket;
				UserDataPacket.PacketId = (UINT16)PACKET_ID::MY_DATA_RESPONSE;
				UserDataPacket.PacketLength = sizeof(USER_DATA_PACKET);
				UserDataPacket.iTotalMatch = Data.iTotalMatch;
				UserDataPacket.iWinCount = Data.iWin;
				UserDataPacket.iLoseCount = Data.iLose;
				SendPacketFunc(clientIndex_, sizeof(USER_DATA_PACKET), (char*)&UserDataPacket);
			}
		}
		else
		{
			// ��й�ȣ ����ġ
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW;
		}

		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
	}
	else 
	{
		//�������� �������� ���и� ��ȯ�Ѵ�.
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
		return;
	}
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	printf("ProcessLoginDBResult. UserIndex: %d\n", clientIndex_);

	auto pBody = (RedisLoginRes*)pPacket_;

	if (pBody->Result == (UINT16)ERROR_CODE::NONE)
	{
		//�α��� �Ϸ�� �����Ѵ�
	}

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	loginResPacket.Result = pBody->Result;
	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
}


void PacketManager::ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomEnterReqPacket = reinterpret_cast<ROOM_ENTER_REQUEST_PACKET*>(pPacket_);
				

	auto pUser= mUserManager->GetUserByConnIdx(clientIndex_);
	ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
	roomEnterResPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_RESPONSE;
	roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);
				
	roomEnterResPacket.Result = mRoomManager->EnterUser(pRoomEnterReqPacket->RoomNumber, pUser);

	if (roomEnterResPacket.Result == (UINT16)ERROR_CODE::NONE)
	{
		Room* pCurRoom = mRoomManager->GetRoomByNumber(pRoomEnterReqPacket->RoomNumber);	
		const std::list<User*>& UserList = pCurRoom->Get_Users();

		roomEnterResPacket.UserCntInRoom = UserList.size();
		roomEnterResPacket.RoomNumber = pRoomEnterReqPacket->RoomNumber;	// ���� �� ��ȣ	

		int iCnt = 0;
		for (auto User : UserList)
		{
			strncpy(roomEnterResPacket.UserIDList[iCnt], User->GetUserId().c_str(), sizeof(roomEnterResPacket.UserIDList[iCnt]));
			roomEnterResPacket.UserIDList[iCnt][sizeof(roomEnterResPacket.UserIDList[iCnt]) - 1] = '\0'; // �� ���� ����		 
			++iCnt;
		}
		
		for (auto User : UserList)
		{
			SendPacketFunc(User->GetNetConnIdx() , sizeof(ROOM_ENTER_RESPONSE_PACKET), (char*)&roomEnterResPacket);
		}
	}
	else
		SendPacketFunc(clientIndex_, sizeof(ROOM_ENTER_RESPONSE_PACKET), (char*)&roomEnterResPacket);

}


void PacketManager::ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	ROOM_LEAVE_RESPONSE_PACKET roomLeaveResPacket;
	roomLeaveResPacket.PacketId = (UINT16)PACKET_ID::ROOM_LEAVE_RESPONSE;
	roomLeaveResPacket.PacketLength = sizeof(ROOM_LEAVE_RESPONSE_PACKET);

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();
				

	//TODO Room���� UserList��ü�� �� Ȯ���ϱ�
	roomLeaveResPacket.Result = mRoomManager->LeaveUser(roomNum, reqUser);
	SendPacketFunc(clientIndex_, sizeof(ROOM_LEAVE_RESPONSE_PACKET), (char*)&roomLeaveResPacket);



	ROOM_LEAVE_NOTIFY_PACKET roomLeaveNotify;
	roomLeaveNotify.PacketId = (UINT16)PACKET_ID::ROOM_LEAVE_NOTIFY;
	roomLeaveNotify.PacketLength = sizeof(ROOM_LEAVE_NOTIFY_PACKET);
	roomLeaveNotify.Result = (UINT16)ERROR_CODE::NONE;

	UINT iCnt = 0;
	auto& Users = mRoomManager->GetRoomByNumber(roomNum)->Get_Users();
	roomLeaveNotify.UserCntInRoom = Users.size();

	for (auto user : Users)
	{	
		strncpy(roomLeaveNotify.UserIDList[iCnt], user->GetUserId().c_str(), sizeof(roomLeaveNotify.UserIDList[iCnt]));
		roomLeaveNotify.UserIDList[iCnt][sizeof(roomLeaveNotify.UserIDList[iCnt]) - 1] = '\0'; // �� ���� ����		 
		++iCnt;

		SendPacketFunc(user->GetNetConnIdx(), sizeof(ROOM_LEAVE_NOTIFY_PACKET), (char*)&roomLeaveNotify);
	}
}


void PacketManager::ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);

	auto pRoomChatReqPacketet = reinterpret_cast<ROOM_CHAT_REQUEST_PACKET*>(pPacket_);
		
	ROOM_CHAT_RESPONSE_PACKET roomChatResPacket;
	roomChatResPacket.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_RESPONSE;
	roomChatResPacket.PacketLength = sizeof(ROOM_CHAT_RESPONSE_PACKET);
	roomChatResPacket.Result = (INT16)ERROR_CODE::NONE;

	auto reqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	auto roomNum = reqUser->GetCurrentRoom();
				
	auto pRoom = mRoomManager->GetRoomByNumber(roomNum);
	if (pRoom == nullptr)
	{
		roomChatResPacket.Result = (INT16)ERROR_CODE::CHAT_ROOM_INVALID_ROOM_NUMBER;
		SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), (char*)&roomChatResPacket);
		return;
	}
		
	SendPacketFunc(clientIndex_, sizeof(ROOM_CHAT_RESPONSE_PACKET), (char*)&roomChatResPacket);

	pRoom->NotifyChat(clientIndex_, reqUser->GetUserId().c_str(), pRoomChatReqPacketet->Message);		
}

void PacketManager::ProcessUserData(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	USER_DATA_PACKET UserDataPacket;
	UserDataPacket.PacketId = (UINT16)PACKET_ID::MY_DATA_RESPONSE;
	UserDataPacket.PacketLength = sizeof(USER_DATA_PACKET);

	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (pReqUser == nullptr)
	{
		return;
	}


	User* pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	const USER_DATA& Data = pUser->Get_MyData();
	UserDataPacket.iTotalMatch = Data.iTotalMatch;
	UserDataPacket.iWinCount= Data.iWin;
	UserDataPacket.iLoseCount = Data.iLose;


	SendPacketFunc(clientIndex_, sizeof(USER_DATA_PACKET), (char*)&UserDataPacket);
}

void PacketManager::ProcessOtherUserData(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	auto pRequestpacket = reinterpret_cast<USER_DATA_REQUEST_PACKET*>(pPacket_);

	INT32 ClinetIndex = mUserManager->FindUserIndexByID(pRequestpacket->userId);
	User* pUser = mUserManager->GetUserByConnIdx(ClinetIndex);
	

	USER_DATA_PACKET UserDataPacket;
	UserDataPacket.PacketId = (UINT16)PACKET_ID::OTHER_USER_DATA_RESPONSE;
	UserDataPacket.PacketLength = sizeof(USER_DATA_PACKET);

	const USER_DATA& Data = pUser->Get_MyData();
	UserDataPacket.iTotalMatch = Data.iTotalMatch;
	UserDataPacket.iWinCount = Data.iWin;
	UserDataPacket.iLoseCount = Data.iLose;

	SendPacketFunc(clientIndex_, sizeof(USER_DATA_PACKET), (char*)&UserDataPacket);
}

void PacketManager::ProcessStartGame(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	auto pRequestpacket = reinterpret_cast<START_GAME_REQUEST_PACKET*>(pPacket_);

	INT32 RoomIndex = pRequestpacket->roomIndex;
	int TurnIndex = pRequestpacket->TurnIndex;
	INT16 BoardSize = pRequestpacket->BoardSize;


	START_GAME_RESPONSE_PACKET packet;
	packet.PacketId = (UINT16)PACKET_ID::START_GAME_RESPONSE_PACKET;
	packet.PacketLength = sizeof(START_GAME_RESPONSE_PACKET);


	INT32 UserIndex = mUserManager->FindUserIndexByID(pRequestpacket->userId);
	if (-1 != UserIndex)
	{
		if (true == mUserManager->FindMeIndexByID(pRequestpacket->userId, clientIndex_))
		{
			packet.Result = (INT16)ERROR_CODE::GAME_FOUND_USER_ME;
		}
		else
		{			
			packet.Result = (INT16)mGameMgr->CheckGamePlay(RoomIndex, TurnIndex, BoardSize, mRoomManager);
			if ((INT16)ERROR_CODE::NONE == packet.Result)
			{
				User* pCompanionUser = mUserManager->GetUserByConnIdx(UserIndex);
				User* pMyUser = mUserManager->GetUserByConnIdx(clientIndex_);

				pCompanionUser->StartPlaying();
				pMyUser->StartPlaying();
			}
		}		
	}
	else
		packet.Result = (INT16)ERROR_CODE::GAME_NOT_FOUND_USER;

	
	
	SendPacketFunc(clientIndex_, sizeof(START_GAME_RESPONSE_PACKET), (char*)&packet);
}

void PacketManager::ProcessStoneLogic(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{
	UNREFERENCED_PARAMETER(packetSize_);
	UNREFERENCED_PARAMETER(pPacket_);

	auto pRequestpacket = reinterpret_cast<PUT_STONE_REQUEST_PACKET*>(pPacket_);

	INT16 row = pRequestpacket->row;
	INT16 col = pRequestpacket->col;
	INT32 roomIndex = pRequestpacket->roomIndex;

	PUT_STONE_RESPONSE_PACKET packet;
	packet.PacketId = (UINT16)PACKET_ID::PUT_STONE_RESPONSE_PACKET;
	packet.PacketLength = sizeof(PUT_STONE_RESPONSE_PACKET);
	packet.row = row;
	packet.col = col;	
	packet.stoneColor = -1;

	if (false == mUserManager->GetUserByConnIdx(clientIndex_)->IsPlaying())
	{
		packet.Result = (INT16)ERROR_CODE::STONE_GAME_OBSERVER;
	}
	else
		packet.Result = (INT16)mGameMgr->CheckPutStone(clientIndex_, row, col, roomIndex, mRoomManager);


	bool isWin = false;
	if ((INT16)ERROR_CODE::NONE == packet.Result)
	{
		packet.stoneColor = mGameMgr->Update_TurnIndex(row, col);

		PUT_STONE_NOTIFY_PACKET Notifypacket;
		Notifypacket.PacketId = (UINT16)PACKET_ID::PUT_STONE_NOTIFY_PACKET;
		Notifypacket.PacketLength = sizeof(PUT_STONE_NOTIFY_PACKET);
		Notifypacket.row = row;
		Notifypacket.col = col;
		Notifypacket.stoneColor = packet.stoneColor;
		Notifypacket.Result = (INT16)ERROR_CODE::NONE;

		isWin = mGameMgr->CheckWin(row, col, packet.stoneColor);

		Room* pRoom = mRoomManager->GetRoomByNumber(roomIndex);
		auto& Users = pRoom->Get_Users();

		USER_DATA_PACKET UserStatPacket{};	// ������ ���� �����͸� �����ִ� ��Ŷ
		UserStatPacket.PacketId = (UINT16)PACKET_ID::MY_DATA_RESPONSE;
		UserStatPacket.PacketLength = sizeof(USER_DATA_PACKET);

		for (auto user : Users)
		{
			if(true == user->IsPlaying() && isWin)	// �̹��Ͽ� ���� �� ������ ���� ��Ŷ�� ���Ͽ� ���� �������� �������׵� �������ش�.
			{
				if (user->GetNetConnIdx() == clientIndex_)
					Notifypacket.Result = (INT16)ERROR_CODE::GAME_WIN;
				else
					Notifypacket.Result = (INT16)ERROR_CODE::GAME_LOSE;

				mGameMgr->GameEnd();
				pRoom->End_GamePlay();
				user->EndPlay();
				
				UpdateUserStat(user, Notifypacket.Result);
				
				USER_DATA Userdata = user->Get_MyData();
				UserStatPacket.iTotalMatch = Userdata.iTotalMatch;
				UserStatPacket.iWinCount = Userdata.iWin;
				UserStatPacket.iLoseCount = Userdata.iLose;
				SendPacketFunc(user->GetNetConnIdx(), sizeof(USER_DATA_PACKET), (char*)&UserStatPacket); // ���� ���� ���׸� ��Ŷ����
			}

			SendPacketFunc(user->GetNetConnIdx(), sizeof(PUT_STONE_NOTIFY_PACKET), (char*)&Notifypacket);
		}
	}

	if(false == isWin)
		SendPacketFunc(clientIndex_, sizeof(PUT_STONE_RESPONSE_PACKET), (char*)&packet);	
}

bool PacketManager::ContainsHangul(const CHAR* str)
{
	while (*str) {
		// Unicode �ѱ� ����: 0xAC00 ~ 0xD7A3
		if (*str >= 0xAC00 && *str <= 0xD7A3) {
			return true;
		}
		++str;
	}
	return false;
}

bool PacketManager::IsInvalidUserID(const CHAR* pUserID)
{
	if (pUserID == nullptr || strlen(pUserID) == 0)
		return true;

	// ���� �˻�
	for (int i = 0; pUserID[i]; ++i) {
		if (isspace(pUserID[i])) {
			return true;
		}
	}

	// �ѱ� ���� ����
	if (ContainsHangul(pUserID)) {
		return true;
	}

	return false;
}

bool PacketManager::LoadUserStat(User* pUser)
{
	char query[512];
	snprintf(query, sizeof(query),
		"SELECT total_matches, wins, losses FROM user_stats WHERE user_id='%s' LIMIT 1;",
		pUser->GetUserId().c_str());

	MYSQL* const sqlconn = mDBMgr->Get_SqlConn();
	int queryResult = mysql_query(sqlconn, query);
	if (queryResult != 0)
	{
		printf("���� ���� ����: %s\n", mysql_error(sqlconn));
		return false;
	}

	MYSQL_RES* result = mysql_store_result(sqlconn);
	if (!result)
	{
		printf("MySQL ��� ���� �Ǵ� ����: %s\n", mysql_error(sqlconn));
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (row == nullptr)
	{
		// �ű� ���� ���� �����Ͱ� ������ �ʱⰪ ����
		mysql_free_result(result);
		return true;
	}

	UINT32 iTotal = atoi(row[0]);
	UINT32 iWin = atoi(row[1]);
	UINT32 iLose = atoi(row[2]);

	USER_DATA Data{};
	Data.iTotalMatch = iTotal;
	Data.iWin = iWin;
	Data.iLose = iLose;
	pUser->LoadUserStat(Data);


	mysql_free_result(result);
	return true;
}

void PacketManager::UpdateUserStat(User* pUser, INT16 iResult)
{
	MYSQL* sqlconn = mDBMgr->Get_SqlConn();

	// user_stats�� �����Ͱ� �ִ��� üũ
	char query[512];
	snprintf(query, sizeof(query),
		"SELECT total_matches, wins, losses FROM user_stats WHERE user_id='%s' LIMIT 1;",
		pUser->GetUserId().c_str());

	int iQueryResult = mysql_query(sqlconn, query);
	if (iQueryResult != 0)
	{
		printf("UpdateUserStat ���� ����: %s\n", mysql_error(sqlconn));
		return;
	}

	MYSQL_RES* result = mysql_store_result(sqlconn);
	if (!result)
	{
		printf("UpdateUserStat ��� ���� �Ǵ� ����: %s\n", mysql_error(sqlconn));
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	//if (row == nullptr)	�̹� ȸ�������Ҷ� db���̺� �����������
	//{
	//	// ������ ������ INSERT
	//	mysql_free_result(result);
	//
	//	snprintf(query, sizeof(query),
	//		"INSERT INTO user_stats (user_id, total_matches, wins, losses) VALUES ('%s', 1, %d, %d);",
	//		pUser->GetUserId().c_str(),
	//		((INT16)ERROR_CODE::GAME_WIN  == iResult) ? 1 : 0,
	//		((INT16)ERROR_CODE::GAME_LOSE == iResult) ? 1 : 0);
	//
	//	if (iQueryResult != 0)
	//	{
	//		printf("UpdateUserStat INSERT ����: %s\n", mysql_error(sqlconn));
	//	}
	//}
	//else
	{
		// ������ ������ UPDATE
		UINT32 iTotal = atoi(row[0]);
		UINT32 iWins = atoi(row[1]);
		UINT32 iLosses = atoi(row[2]);

		mysql_free_result(result);

		iTotal++;
		if ((INT16)ERROR_CODE::GAME_WIN == iResult)
			iWins++;
		else 
			iLosses++;

		snprintf(query, sizeof(query),
			"UPDATE user_stats SET total_matches=%d, wins=%d, losses=%d, updated_at=NOW() WHERE user_id='%s';",
			iTotal, iWins, iLosses, pUser->GetUserId().c_str());

		if (mysql_query(sqlconn, query) != 0)
		{
			printf("UpdateUserStat UPDATE ����: %s\n", mysql_error(sqlconn));
		}
		else
		{
			USER_DATA Data{};
			Data.iTotalMatch = iTotal;
			Data.iWin = iWins;
			Data.iLose = iLosses;
			pUser->LoadUserStat(Data);

			printf("[DEBUG] user_id: [%s]\n", pUser->GetUserId().c_str());
			printf("[DEBUG] UPDATE ����: %s\n", query);
		}
	}
}

