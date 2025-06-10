#pragma once
#include <unordered_map>

#include "ErrorCode.h"
#include "User.h"

class UserManager
{
public:
	UserManager() = default;
	~UserManager() = default;

	void Init(const INT32 maxUserCount_)
	{
		mMaxUserCnt = maxUserCount_;
		mUserObjPool = std::vector<User*>(mMaxUserCnt);

		for (auto i = 0; i < mMaxUserCnt; i++)
		{
			mUserObjPool[i] = new User();
			mUserObjPool[i]->Init(i);			
		}
	}

	INT32 GetCurrentUserCnt() { return mCurrentUserCnt; }

	INT32 GetMaxUserCnt() { return mMaxUserCnt; }
	
	ERROR_CODE AddUser(char* userID_, int clientIndex_)
	{
		auto user_idx = clientIndex_;

		mUserObjPool[user_idx]->SetLogin(userID_);		// 유저 풀에서 걍 유저를 생성해놓은다음 그 유저가 잇는 인덱스 == 클라 인덱스
		mUserIDDictionary.insert(std::pair< char*, int>(userID_, clientIndex_));

		IncreaseUserCnt();

		return ERROR_CODE::NONE;
	}
		
	INT32 FindUserIndexByID(const char* userID_)	
	{
		if (auto res = mUserIDDictionary.find(userID_); res != mUserIDDictionary.end())
		{
			return (*res).second;		// 유저를 발견햇단 소리고
		}
			
		return -1;						// 없어
	}
		
	void DeleteUserInfo(User* user_)
	{
		mUserIDDictionary.erase(user_->GetUserId());
		user_->Clear();

		DecreaseUserCnt();
	}

	User* GetUserByConnIdx(INT32 clientIndex_)
	{
		return mUserObjPool[clientIndex_];
	}


	bool IsUserByLogin(INT32 clientIndex_)
	{
		if (User::DOMAIN_STATE::LOGIN == mUserObjPool[clientIndex_]->GetDomainState())
		{
			return true;
		}

		return false;
	}

	bool IsUserByRoom(INT32 clientIndex_)
	{
		if (User::DOMAIN_STATE::ROOM == mUserObjPool[clientIndex_]->GetDomainState())
		{
			return true;
		}

		return false;
	}


private:
	void IncreaseUserCnt() { mCurrentUserCnt++; }
	void DecreaseUserCnt()
	{
		if (mCurrentUserCnt > 0)
		{
			mCurrentUserCnt--;
		}
	}

private:
	INT32 mMaxUserCnt = 0;
	INT32 mCurrentUserCnt = 0;

	std::vector<User*> mUserObjPool; //vector로
	std::unordered_map<std::string, int> mUserIDDictionary;
};
