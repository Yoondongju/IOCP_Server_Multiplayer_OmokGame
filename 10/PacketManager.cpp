#include <utility>
#include <cstring>


#include "MyDBManager.h"

#include "UserManager.h"
#include "RoomManager.h"
#include "PacketManager.h"
#include "RedisManager.h"



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
				
	mRecvFuntionDictionary[(int)PACKET_ID::USER_DATA_REQUEST] = &PacketManager::ProcessUserData;

	CreateCompent(maxClient_);

	mRedisMgr = new RedisManager;// std::make_unique<RedisManager>();
	mDBMgr = new MyDBManager;
	if (!mDBMgr->IsConnected())
		mDBMgr->Connect();
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

		userIndex = mInComingPacketUserIndex.front();
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

	char query[512];
	snprintf(query, sizeof(query),
		"INSERT INTO users (user_id, password_hash) VALUES ('%s', '%s')",		// DB에 저장
		pRegisterPacket->UserID, pRegisterPacket->UserPW);


	MYSQL* const sqlconn = mDBMgr->Get_SqlConn();

	int queryResult = mysql_query(sqlconn, query);

	REGISTER_RESPONSE_PACKET resPacket{};
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
		resPacket.Result = 0; // 성공
	}

	resPacket.PacketLength = sizeof(resPacket);

	SendPacketFunc(clientIndex_, sizeof(resPacket), (char*)&resPacket);
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
	
			mUserManager->AddUser(pLoginReqPacket->UserID, mUserManager->GetCurrentUserCnt());
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

	auto pRoomChatReqPacketet = reinterpret_cast<USER_DATA_PACKET*>(pPacket_);


	USER_DATA_PACKET UserDataPacket;
	UserDataPacket.PacketId = (UINT16)PACKET_ID::USER_DATA_RESPONSE;
	UserDataPacket.PacketLength = sizeof(USER_DATA_PACKET);

	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);
	if (pReqUser == nullptr)
	{
		return;
	}


	strncpy(UserDataPacket.userId, pReqUser->GetUserId().c_str(), sizeof(UserDataPacket.userId));
	UserDataPacket.userId[sizeof(UserDataPacket.userId) - 1] = '\0'; // 널 종료 보장
	UserDataPacket.userIndex = pReqUser->GetNetConnIdx();
	UserDataPacket.domainState = (UINT32)pReqUser->GetDomainState();
	UserDataPacket.roomIndex = pReqUser->GetCurrentRoom();	// -1가면 방없음임

	SendPacketFunc(clientIndex_, sizeof(USER_DATA_PACKET), (char*)&UserDataPacket);
}

