#pragma once

//TODO ���� �ڵ� �ߺ� ������� �ʵ��� �Ѵ�
enum class ERROR_CODE : unsigned short
{
	NONE = 0,


	USER_MGR_INVALID_USER_INDEX = 11,      // ���� ����: ��ȿ���� ���� ���� �ε���
	USER_MGR_INVALID_USER_UNIQUEID = 12,  // ���� ����: ��ȿ���� ���� ���� ���� ID

	LOGIN_USER_ALREADY = 31,               // �α���: �̹� �α��ε� ����
	LOGIN_USER_USED_ALL_OBJ = 32,          // �α���: ��� ������ ���� ������Ʈ ��� ����
	LOGIN_USER_INVALID_PW = 33,            // �α���: �߸��� ��й�ȣ

	NEW_ROOM_USED_ALL_OBJ = 41,            // �� �� ����: ��� ������ �� ������Ʈ ��� ����
	NEW_ROOM_FAIL_ENTER = 42,              // �� �� ����: ���� ����

	ENTER_ROOM_NOT_FINDE_USER = 51,        // �� ����: ������ ã�� �� ����
	ENTER_ROOM_INVALID_USER_STATUS = 52,   // �� ����: ���� ���°� ��ȿ���� ����
	ENTER_ROOM_NOT_USED_STATUS = 53,       // �� ����: ��� ������ ���� ����
	ENTER_ROOM_FULL_USER = 54,             // �� ����: �濡 ������ ���� ��

	ROOM_INVALID_INDEX = 61,               // ��: ��ȿ���� ���� �� �ε���
	ROOM_NOT_USED = 62,                    // ��: ��� ������ ����
	ROOM_TOO_MANY_PACKET = 63,             // ��: �ʹ� ���� ��Ŷ �߻�

	LEAVE_ROOM_INVALID_ROOM_INDEX = 71,   // �� ������: ��ȿ���� ���� �� �ε���

	CHAT_ROOM_INVALID_ROOM_NUMBER = 81,   // ä��: ��ȿ���� ���� ä�ù� ��ȣ

	LOGIN_DB_ERROR = 99,                  // �α���: �����ͺ��̽� ����
	LOGIN_USER_NOT_FOUND = 100,           // �α���: ������ ã�� �� ����


	STONE_ALREADY_EXISTS = 111,         // �̹� ���� ���� ����
	//STONE_VIOLATES_33_RULE = 112,     // ���� ����� �ϴ� ���
	STONE_NOT_YOUR_TURN = 113,          // �ڱ� ���ʰ� �ƴ�
	STONE_OUT_OF_BOUNDS = 114,          // ������ ��� ��ǥ
	STONE_GAME_NOT_PLAYING = 115,       // ������ ���������� ���� 
	STONE_GAME_ALREADY_PLAYING = 116,	// �̹� ������ 
};
