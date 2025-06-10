#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


struct RawPacketData
{
	UINT32 ClientIndex = 0;
	UINT32 DataSize = 0;
	char* pPacketData = nullptr;

	void Set(RawPacketData& vlaue)
	{
		ClientIndex = vlaue.ClientIndex;
		DataSize = vlaue.DataSize;

		pPacketData = new char[vlaue.DataSize];
		CopyMemory(pPacketData, vlaue.pPacketData, vlaue.DataSize);
	}

	void Set(UINT32 clientIndex_, UINT32 dataSize_, char* pData)
	{
		ClientIndex = clientIndex_;
		DataSize = dataSize_;

		pPacketData = new char[dataSize_];
		CopyMemory(pPacketData, pData, dataSize_);
	}

	void Release()
	{
		delete pPacketData;
	}
};


struct PacketInfo
{
	UINT32 ClientIndex = 0;
	UINT16 PacketId = 0;
	UINT16 DataSize = 0;
	char* pDataPtr = nullptr;
};


enum class  PACKET_ID : UINT16   // 전송 크기를 줄일 수 있고, 구조체 정렬도 최적화할 수 있음.
{
	// 시스템관련 1 ~ 99 
	// DB 관련 100 ~ 199
	// 클라 로직 관련 200 ~ 

	//SYSTEM
	SYS_USER_CONNECT = 11,
	SYS_USER_DISCONNECT = 12,
	SYS_END = 30,

	//DB
	DB_END = 199,

	//Client
	REGISTER_REQUEST = 200,		// 회원가입
	REGISTER_RESPONSE = 201,

	LOGIN_REQUEST = 202,		
	LOGIN_RESPONSE = 203,

	ROOM_ENTER_REQUEST = 206,
	ROOM_ENTER_RESPONSE = 207,

	ROOM_LEAVE_REQUEST = 215,
	ROOM_LEAVE_RESPONSE = 216,
	ROOM_LEAVE_NOTIFY = 217,

	ROOM_CHAT_REQUEST = 221,
	ROOM_CHAT_RESPONSE = 222,
	ROOM_CHAT_NOTIFY = 223,

	USER_DATA_REQUEST = 224,
	USER_DATA_RESPONSE = 225,


	START_GAME_REQUEST_PACKET = 248,
	START_GAME_RESPONSE_PACKET = 249,

	PUT_STONE_REQUEST_PACKET = 250,
	PUT_STONE_RESPONSE_PACKET = 251,
	PUT_STONE_NOTIFY_PACKET = 252

};


#pragma pack(push,1)
struct PACKET_HEADER
{
	UINT16 PacketLength;
	UINT16 PacketId;
	UINT8 Type; //압축여부 암호화여부 등 속성을 알아내는 값
};

const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);

//- 로그인 요청
const int MAX_USER_COUNT = 20;
const int MAX_USER_ID_LEN = 32;
const int MAX_USER_PW_LEN = 32;

struct REGISTER_REQUEST_PACKET : public PACKET_HEADER
{
	char UserID[MAX_USER_ID_LEN + 1];
	char UserPW[MAX_USER_PW_LEN + 1];
};

struct REGISTER_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16 Result; // 0: 성공, 1: 실패 등
};

struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char UserID[MAX_USER_ID_LEN + 1];
	char UserPW[MAX_USER_PW_LEN + 1];
};
const size_t LOGIN_REQUEST_PACKET_SZIE = sizeof(LOGIN_REQUEST_PACKET);


struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16 Result;
};



//- 룸에 들어가기 요청
//const int MAX_ROOM_TITLE_SIZE = 32;
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	INT32 RoomNumber;
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	INT32 RoomNumber;
	INT32 UserCntInRoom;
	INT16 Result;	
	char UserIDList[MAX_USER_COUNT][MAX_USER_ID_LEN + 1] = { 0 };
};


//- 룸 나가기 요청
struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
};

struct ROOM_LEAVE_NOTIFY_PACKET : public PACKET_HEADER
{
	INT32 UserCntInRoom;
	INT16 Result;
	char UserIDList[MAX_USER_COUNT][MAX_USER_ID_LEN + 1] = { 0 };
};



// 룸 채팅
const int MAX_CHAT_MSG_SIZE = 256;
struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
{
	char Message[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};

struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
};

struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
{
	char UserID[MAX_USER_ID_LEN + 1] = { 0, };
	char Msg[MAX_CHAT_MSG_SIZE + 1] = { 0, };
};

// 유저 정보 패킷
struct USER_DATA_PACKET : public PACKET_HEADER
{
	UINT32 userIndex;
	char userId[64];       
	INT32 roomIndex;
	UINT32 domainState;      // DOMAIN_STATE enum값 (예: 0=NONE,1=LOGIN,2=ROOM)
};

struct START_GAME_REQUEST_PACKET : public PACKET_HEADER
{
	char userId[64];
	INT32 roomIndex;
	int TurnIndex;
	INT16 BoardSize;
};

struct START_GAME_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
};



struct PUT_STONE_REQUEST_PACKET : public PACKET_HEADER
{
	INT32 roomIndex;
	INT16 row;
	INT16 col;
};

struct PUT_STONE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 row;
	INT16 col;
	INT16 Result;

	INT8 stoneColor; // 1: 흑, 2: 백
};

struct PUT_STONE_NOTIFY_PACKET : public PACKET_HEADER
{
	INT16 row;
	INT16 col;
	INT16 Result;
	INT8 stoneColor; // 1: 흑, 2: 백
};



#pragma pack(pop) //위에 설정된 패킹설정이 사라짐




