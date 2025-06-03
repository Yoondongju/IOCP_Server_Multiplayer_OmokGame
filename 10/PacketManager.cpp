#include <utility>
#include <cstring>


#include "UserManager.h"
#include "RoomManager.h"
#include "PacketManager.h"
#include "RedisManager.h"




void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFuntionDictionary = std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFuntionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;

	mRecvFuntionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFuntionDictionary[(int)RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;
	
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_ENTER_REQUEST] = &PacketManager::ProcessEnterRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_LEAVE_REQUEST] = &PacketManager::ProcessLeaveRoom;
	mRecvFuntionDictionary[(int)PACKET_ID::ROOM_CHAT_REQUEST] = &PacketManager::ProcessRoomChatMessage;
				
	CreateCompent(maxClient_);

	mRedisMgr = new RedisManager;// std::make_unique<RedisManager>();
}

void PacketManager::CreateCompent(const UINT32 maxClient_)
{
	mUserManager = new UserManager;
	mUserManager->Init(maxClient_);

		
	UINT32 startRoomNummber = 0;
	UINT32 maxRoomCount = 10;
	UINT32 maxRoomUserCount = 4;
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
		//RedisLoginReq dbReq;
		//CopyMemory(dbReq.UserID, pLoginReqPacket->UserID, (MAX_USER_ID_LEN + 1));
		//CopyMemory(dbReq.UserPW, pLoginReqPacket->UserPW, (MAX_USER_PW_LEN + 1));
		//
		//RedisTask task;
		//task.UserIndex = clientIndex_;
		//task.TaskID = RedisTaskID::REQUEST_LOGIN;
		//task.DataSize = sizeof(RedisLoginReq);
		//task.pData = new char[task.DataSize];
		//CopyMemory(task.pData, (char*)&dbReq, task.DataSize);
		//mRedisMgr->PushTask(task);

		
		printf("MySql에서 아이디 찾는중.. >> %s\n", pUserID);

		m_mysqlConn = mysql_init(nullptr);
		if (m_mysqlConn == nullptr) {
			std::cout << "MySQL 초기화 실패\n";
			return;
		}
		if (mysql_real_connect(m_mysqlConn, "localhost", "root", "0928", "game_db", 3306, nullptr, 0)) {
			std::cout << "MySQL 연결 성공!\n";
		}
		else 
		{
			std::cout << "MySQL 연결 실패: " << mysql_error(m_mysqlConn) << "\n";
			mysql_close(m_mysqlConn);
			return;
		}


		// MySQL 쿼리 준비
		char query[512];
		snprintf(query, sizeof(query),
			"SELECT password_hash FROM users WHERE user_id='%s' LIMIT 1;",
			pUserID);


		if (mysql_query(m_mysqlConn, query) != 0)  // 쿼리 실패시
		{
			printf("MySQL 쿼리 실패: %s\n", mysql_error(m_mysqlConn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

			mysql_close(m_mysqlConn);
			return;
		}

		MYSQL_RES* result = mysql_store_result(m_mysqlConn);
		if (!result)
		{
			printf("MySQL 결과 없음 또는 오류: %s\n", mysql_error(m_mysqlConn));
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_DB_ERROR;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

			mysql_close(m_mysqlConn);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(result);
		if (row == nullptr)
		{
			// 아이디 없음
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND;
			SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), (char*)&loginResPacket);

			mysql_free_result(result);
			mysql_close(m_mysqlConn);
			return;
		}

		const char* storedHash = row[0];
		mysql_free_result(result);
		mysql_close(m_mysqlConn);

		// 여기서 비밀번호 검증
		// 실제 서비스라면 bcrypt 같은 해시 검증 해야 하지만, 지금은 단순 비교 예시
		if (strcmp(storedHash, pLoginReqPacket->UserPW) == 0)
		{
			// 로그인 성공
			loginResPacket.Result = (UINT16)ERROR_CODE::NONE;

		}
		else
		{
			// 비밀번호 불일치
			loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW;
		}

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
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (!pReqUser || pReqUser == nullptr) 
	{
		return;
	}
				
	ROOM_ENTER_RESPONSE_PACKET roomEnterResPacket;
	roomEnterResPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_RESPONSE;
	roomEnterResPacket.PacketLength = sizeof(ROOM_ENTER_RESPONSE_PACKET);
				
	roomEnterResPacket.Result = mRoomManager->EnterUser(pRoomEnterReqPacket->RoomNumber, pReqUser);

	SendPacketFunc(clientIndex_, sizeof(ROOM_ENTER_RESPONSE_PACKET), (char*)&roomEnterResPacket);
	printf("Response Packet Sended");
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

