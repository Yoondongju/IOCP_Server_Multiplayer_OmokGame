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

		
	UINT32 startRoomNummber = 0;	// 방 번호
	UINT32 maxRoomCount = 10;		// 방 최대 갯수
	UINT32 maxRoomUserCount = 4;	// 방 최대 접속 인원
	mRoomManager = new RoomManager;
	mRoomManager->SendPacketFunc = SendPacketFunc;
	mRoomManager->Init(startRoomNummber, maxRoomCount, maxRoomUserCount);
}

bool PacketManager::Run()
{	
	if (mRedisMgr->Run("127.0.0.1", 6379, 1) == false)	// Redis초기화및 Redis처리 스레드 생성
	{
		return false;
	}

	//이 부분을 패킷 처리 부분으로 이동 시킨다.
	mIsRunProcessThread = true;
	mProcessThread = std::thread([this]() { ProcessPacket(); });

	printf("패킷처리 스레드 시작..\n");

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

		userIndex = mInComingPacketUserIndex.front();	// 이게 패킷보낸 클라의 인덱스다!
		mInComingPacketUserIndex.pop_front();
	}

	auto pUser = mUserManager->GetUserByConnIdx(userIndex);
	auto packetData = pUser->GetPacket();	// 유저가 들고잇는 패킷을 가져온다
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
		"INSERT INTO users (user_id, password_hash) VALUES ('%s', '%s')",		// DB에 저장
		pRegisterPacket->UserID, pRegisterPacket->UserPW);


	MYSQL* const sqlconn = mDBMgr->Get_SqlConn();
	int queryResult = mysql_query(sqlconn, query);

	
	resPacket.PacketId = (UINT16)PACKET_ID::REGISTER_RESPONSE;

	if (queryResult != 0)
	{
		// 에러 로그
		std::cout << "MySQL 쿼리 실패: " << mysql_error(sqlconn) << "\n";

		// 중복 ID일 경우 (Error code 1062) 구분 가능
		if (mysql_errno(sqlconn) == 1062)
		{
			resPacket.Result = 2; // ID 중복 에러 코드 예시
		}
		else
		{
			resPacket.Result = 1; // 일반 실패
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
			std::cout << "전적 초기화 쿼리 실패: " << mysql_error(sqlconn) << "\n";
		}

		resPacket.Result = 0; // 성공
	}

	SendPacketFunc(clientIndex_, sizeof(REGISTER_RESPONSE_PACKET), (char*)&resPacket);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_)
{ 
	if (LOGIN_REQUEST_PACKET_SZIE != packetSize_)
	{
		printf("잘못된 크기의 패킷사이즈입니다.\n");
		return;
	}

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_);

	auto pUserID = pLoginReqPacket->UserID;
	printf("로그인 요청한 유저아이디 >> %s\n", pUserID);

	LOGIN_RESPONSE_PACKET loginResPacket;
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	
	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt()) 
	{ 
		//접속자수가 최대수를 차지해서 접속불가
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET) , (char*)&loginResPacket);
		return;
	}

	//여기에서 이미 접속된 유저인지 확인하고, 접속된 유저라면 실패한다.
	if (mUserManager->FindUserIndexByID(pUserID) == -1)		// 신입
	{ 
		MYSQL* const sqlconn = mDBMgr->Get_SqlConn();
		printf("MySql에서 아이디 찾는중.. >> %s\n", pUserID);

		char query[512];
		snprintf(query, sizeof(query),
			"SELECT password_hash FROM users WHERE user_id='%s' LIMIT 1;",
			pUserID);

		if (mysql_query(sqlconn, query) != 0)  // 쿼리 실패시
		{
			printf("MySQL 쿼리 실패: %s\n", mysql_error(sqlconn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
			return;
		}

		MYSQL_RES* result = mysql_store_result(sqlconn);
		if (!result)
		{
			printf("MySQL 결과 없음 또는 오류: %s\n", mysql_error(sqlconn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(result);
		if (row == nullptr)
		{
			// 아이디 없음
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

			mysql_free_result(result);
			return;
		}

		const char* storedHash = row[0];
		mysql_free_result(result);

		// 여기서 비밀번호 검증
		// 실제 서비스라면 bcrypt 같은 해시 검증 해야 하지만, 지금은 단순 비교 예시
		if (strcmp(storedHash, pLoginReqPacket->UserPW) == 0)
		{
			// 로그인 성공
			loginResPacket.Result = (UINT16)ERROR_CODE::NONE;
			mUserManager->AddUser(pLoginReqPacket->UserID, clientIndex_);

			User* pUser = mUserManager->GetUserByConnIdx(clientIndex_);
			const USER_DATA& Data = pUser->Get_MyData();

			if (false == LoadUserStat(pUser))
			{
				printf("유저 전적 불러오기 실패: %s\n", pUserID);
			}
			else
			{
				printf("유저 전적 불러오기 성공: %s (승:%d 패:%d 총:%d)\n",
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
			// 비밀번호 불일치
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW;
		}

		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);
	}
	else 
	{
		//접속중인 유저여서 실패를 반환한다.
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
		//로그인 완료로 변경한다
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
		roomEnterResPacket.RoomNumber = pRoomEnterReqPacket->RoomNumber;	// 현재 방 번호	

		int iCnt = 0;
		for (auto User : UserList)
		{
			strncpy(roomEnterResPacket.UserIDList[iCnt], User->GetUserId().c_str(), sizeof(roomEnterResPacket.UserIDList[iCnt]));
			roomEnterResPacket.UserIDList[iCnt][sizeof(roomEnterResPacket.UserIDList[iCnt]) - 1] = '\0'; // 널 종료 보장		 
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
				

	//TODO Room안의 UserList객체의 값 확인하기
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
		roomLeaveNotify.UserIDList[iCnt][sizeof(roomLeaveNotify.UserIDList[iCnt]) - 1] = '\0'; // 널 종료 보장		 
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

		USER_DATA_PACKET UserStatPacket{};	// 유저의 전적 데이터를 보내주는 패킷
		UserStatPacket.PacketId = (UINT16)PACKET_ID::MY_DATA_RESPONSE;
		UserStatPacket.PacketLength = sizeof(USER_DATA_PACKET);

		for (auto user : Users)
		{
			if(true == user->IsPlaying() && isWin)	// 이번턴에 돌을 둔 유저가 보낸 패킷에 의하여 나와 게임중인 유저한테도 영향을준다.
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
				SendPacketFunc(user->GetNetConnIdx(), sizeof(USER_DATA_PACKET), (char*)&UserStatPacket); // 나와 상대방 한테만 패킷보냄
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
		// Unicode 한글 범위: 0xAC00 ~ 0xD7A3
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

	// 공백 검사
	for (int i = 0; pUserID[i]; ++i) {
		if (isspace(pUserID[i])) {
			return true;
		}
	}

	// 한글 포함 여부
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
		printf("전적 쿼리 실패: %s\n", mysql_error(sqlconn));
		return false;
	}

	MYSQL_RES* result = mysql_store_result(sqlconn);
	if (!result)
	{
		printf("MySQL 결과 없음 또는 오류: %s\n", mysql_error(sqlconn));
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (row == nullptr)
	{
		// 신규 유저 전적 데이터가 없으면 초기값 유지
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

	// user_stats에 데이터가 있는지 체크
	char query[512];
	snprintf(query, sizeof(query),
		"SELECT total_matches, wins, losses FROM user_stats WHERE user_id='%s' LIMIT 1;",
		pUser->GetUserId().c_str());

	int iQueryResult = mysql_query(sqlconn, query);
	if (iQueryResult != 0)
	{
		printf("UpdateUserStat 쿼리 실패: %s\n", mysql_error(sqlconn));
		return;
	}

	MYSQL_RES* result = mysql_store_result(sqlconn);
	if (!result)
	{
		printf("UpdateUserStat 결과 없음 또는 오류: %s\n", mysql_error(sqlconn));
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	//if (row == nullptr)	이미 회원가입할때 db테이블 만들어주잔항
	//{
	//	// 데이터 없으면 INSERT
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
	//		printf("UpdateUserStat INSERT 실패: %s\n", mysql_error(sqlconn));
	//	}
	//}
	//else
	{
		// 데이터 있으면 UPDATE
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
			printf("UpdateUserStat UPDATE 실패: %s\n", mysql_error(sqlconn));
		}
		else
		{
			USER_DATA Data{};
			Data.iTotalMatch = iTotal;
			Data.iWin = iWins;
			Data.iLose = iLosses;
			pUser->LoadUserStat(Data);

			printf("[DEBUG] user_id: [%s]\n", pUser->GetUserId().c_str());
			printf("[DEBUG] UPDATE 쿼리: %s\n", query);
		}
	}
}

