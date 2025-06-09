#pragma once

//TODO 에러 코드 중복 사용하지 않도록 한다
enum class ERROR_CODE : unsigned short
{
	NONE = 0,


	USER_MGR_INVALID_USER_INDEX = 11,      // 유저 관리: 유효하지 않은 유저 인덱스
	USER_MGR_INVALID_USER_UNIQUEID = 12,  // 유저 관리: 유효하지 않은 유저 고유 ID

	LOGIN_USER_ALREADY = 31,               // 로그인: 이미 로그인된 유저
	LOGIN_USER_USED_ALL_OBJ = 32,          // 로그인: 사용 가능한 유저 오브젝트 모두 사용됨
	LOGIN_USER_INVALID_PW = 33,            // 로그인: 잘못된 비밀번호

	NEW_ROOM_USED_ALL_OBJ = 41,            // 새 방 생성: 사용 가능한 방 오브젝트 모두 사용됨
	NEW_ROOM_FAIL_ENTER = 42,              // 새 방 생성: 입장 실패

	ENTER_ROOM_NOT_FINDE_USER = 51,        // 방 입장: 유저를 찾을 수 없음
	ENTER_ROOM_INVALID_USER_STATUS = 52,   // 방 입장: 유저 상태가 유효하지 않음
	ENTER_ROOM_NOT_USED_STATUS = 53,       // 방 입장: 사용 중이지 않은 상태
	ENTER_ROOM_FULL_USER = 54,             // 방 입장: 방에 유저가 가득 참

	ROOM_INVALID_INDEX = 61,               // 방: 유효하지 않은 방 인덱스
	ROOM_NOT_USED = 62,                    // 방: 사용 중이지 않음
	ROOM_TOO_MANY_PACKET = 63,             // 방: 너무 많은 패킷 발생

	LEAVE_ROOM_INVALID_ROOM_INDEX = 71,   // 방 나가기: 유효하지 않은 방 인덱스

	CHAT_ROOM_INVALID_ROOM_NUMBER = 81,   // 채팅: 유효하지 않은 채팅방 번호

	LOGIN_DB_ERROR = 99,                  // 로그인: 데이터베이스 에러
	LOGIN_USER_NOT_FOUND = 100,           // 로그인: 유저를 찾을 수 없음


	STONE_ALREADY_EXISTS = 111,         // 이미 돌이 놓여 있음
	//STONE_VIOLATES_33_RULE = 112,     // 구현 어려움 일단 대기
	STONE_NOT_YOUR_TURN = 113,          // 자기 차례가 아님
	STONE_OUT_OF_BOUNDS = 114,          // 범위를 벗어난 좌표
	STONE_GAME_NOT_PLAYING = 115,       // 게임이 진행중이지 않음 
	STONE_GAME_ALREADY_PLAYING = 116,	// 이미 게임중 
};
