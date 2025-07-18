#pragma once

#include "Packet.h"

#include <unordered_map>
#include <deque>
#include <functional>
#include <thread>
#include <mutex>



class UserManager;
class RoomManager;
class RedisManager;
class MyDBManager;
class GameManager;
class User;


class PacketManager {
public:
	PacketManager() = default;
	~PacketManager() = default;

	void Init(const UINT32 maxClient_);

	bool Run();

	void End();

	void ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_);

	void PushSystemPacket(PacketInfo packet_);
		
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;


private:
	void CreateCompent(const UINT32 maxClient_);

	void ClearConnectionInfo(INT32 clientIndex_);

	void EnqueuePacketData(const UINT32 clientIndex_);
	PacketInfo DequePacketData();

	PacketInfo DequeSystemPacketData();


	void ProcessPacket();

	void ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_);

	void ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	
	
	void ProcessRegister(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	
	void ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	
	void ProcessUserData(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessOtherUserData(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	
	void ProcessStartGame(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessStoneLogic(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);


	bool ContainsHangul(const CHAR* str);
	bool IsInvalidUserID(const CHAR* pUserID);


	bool LoadUserStat(User* pUser);
	void UpdateUserStat(User* pUser, INT16 iResult);


	typedef void(PacketManager::* PROCESS_RECV_PACKET_FUNCTION)(UINT32, UINT16, char*);
	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFuntionDictionary;

	UserManager* mUserManager;
	RoomManager* mRoomManager;	
	RedisManager* mRedisMgr;
	MyDBManager* mDBMgr;
	GameManager* mGameMgr;

		
	std::function<void(int, char*)> mSendMQDataFunc;	// �Ⱦ���?


	bool mIsRunProcessThread = false;
	
	std::thread mProcessThread;
	
	std::mutex mLock;
	
	std::deque<UINT32> mInComingPacketUserIndex;

	std::deque<PacketInfo> mSystemPacketQueue;

};


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