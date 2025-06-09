#pragma once

#include "UserManager.h"
#include "Packet.h"

#include "ErrorCode.h"

#include <functional>


class Room 
{
public:
	Room() = default;
	~Room() = default;


	ERROR_CODE IsPlaying()
	{
		if (false == mIsPlaying)
		{
			return ERROR_CODE::STONE_GAME_NOT_PLAYING;
		}
		return ERROR_CODE::NONE;
	}

	ERROR_CODE IsValidPosition(INT16 row, INT16 col, int** Board ,INT16 BoardSize)
	{		
		if (row < 0 || row > BoardSize || col < 0 || col > BoardSize)
		{
			return ERROR_CODE::STONE_OUT_OF_BOUNDS;
		}
		else
			return ERROR_CODE::NONE;
	}

	ERROR_CODE IsStonePlaced(INT16 row, INT16 col, int** Board, INT16 BoardSize)
	{
		if( 0 != Board[row][col])
			return ERROR_CODE::STONE_ALREADY_EXISTS;
		else 
			return ERROR_CODE::NONE;
	}

	ERROR_CODE IsUserTurn(UINT32 clientIndex)
	{
		if (clientIndex != mCurrentTurnClientIndex)
			return ERROR_CODE::STONE_NOT_YOUR_TURN;

		return ERROR_CODE::NONE;
	}



	void Change_CurTurnClientIndex(int ClientIndex) { mCurrentTurnClientIndex = ClientIndex; }
	void Start_GamePlay() { mIsPlaying = true; }



	INT32 GetMaxUserCount() { return mMaxUserCount; }

	INT32 GetCurrentUserCount() { return mCurrentUserCount; }

	INT32 GetRoomNumber() { return mRoomNum; }

	const std::list<User*>& Get_Users() { return mUserList; }
	
	void Init(const INT32 roomNum_, const INT32 maxUserCount_)
	{
		mRoomNum = roomNum_;
		mMaxUserCount = maxUserCount_;
	}

	UINT16 EnterUser(User* user_)
	{
		if (mCurrentUserCount >= mMaxUserCount)
		{
			return (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER;	// πÊ ≤À¬˛æÓ
		}

		User::DOMAIN_STATE State = user_->GetDomainState();
		if (User::DOMAIN_STATE::LOGIN != State)
		{
			return (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND;
		}
	

		mUserList.push_back(user_);
		++mCurrentUserCount;

		user_->EnterRoom(mRoomNum);
		return (UINT16)ERROR_CODE::NONE;
	}
		
	void LeaveUser(User* leaveUser_)
	{
		--mCurrentUserCount;

		mUserList.remove_if([leaveUserId = leaveUser_->GetUserId()](User* pUser) {
			return leaveUserId == pUser->GetUserId();
		});
	}
						
	void NotifyChat(INT32 clientIndex_, const char* userID_, const char* msg_)
	{
		ROOM_CHAT_NOTIFY_PACKET roomChatNtfyPkt;
		roomChatNtfyPkt.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_NOTIFY;
		roomChatNtfyPkt.PacketLength = sizeof(roomChatNtfyPkt);

		CopyMemory(roomChatNtfyPkt.Msg, msg_, sizeof(roomChatNtfyPkt.Msg));
		CopyMemory(roomChatNtfyPkt.UserID, userID_, sizeof(roomChatNtfyPkt.UserID));
		SendToAllUser(sizeof(roomChatNtfyPkt), (char*)&roomChatNtfyPkt, clientIndex_, false);
	}
		
		
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

private:
	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		for (auto pUser : mUserList)
		{
			if (pUser == nullptr) {
				continue;
			}

			if (exceptMe && pUser->GetNetConnIdx() == passUserIndex_) {
				continue;
			}

			SendPacketFunc((UINT32)pUser->GetNetConnIdx(), (UINT32)dataSize_, data_);
		}
	}


	INT32 mRoomNum = -1;
	int mCurrentTurnClientIndex = -1;	// ¥©±∏ ≈œ¿Ã¥œ 
	bool mIsPlaying = false;

	std::list<User*> mUserList;
		
	INT32 mMaxUserCount = 0;

	UINT16 mCurrentUserCount = 0;
};
