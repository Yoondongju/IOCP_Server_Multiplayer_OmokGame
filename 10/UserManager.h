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

		mUserObjPool[user_idx]->SetLogin(userID_);		// ���� Ǯ���� �� ������ �����س������� �� ������ �մ� �ε��� == Ŭ�� �ε���
		mUserIDDictionary.insert(std::pair< char*, int>(userID_, clientIndex_));

		IncreaseUserCnt();

		return ERROR_CODE::NONE;
	}
		
	INT32 FindUserIndexByID(const char* userID_)	
	{
		if (auto res = mUserIDDictionary.find(userID_); res != mUserIDDictionary.end())
		{
			return (*res).second;		// ������ �߰��޴� �Ҹ���
		}
			
		return -1;						// ����
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

	std::vector<User*> mUserObjPool; //vector��
	std::unordered_map<std::string, int> mUserIDDictionary;
};
