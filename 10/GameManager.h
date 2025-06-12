#pragma once

#include <unordered_map>
#include <memory>
#include "Room.h"

class RoomManager;

class GameManager
{
public:
    GameManager() = default;
    ~GameManager() = default;

    bool Init(int maxRooms);

    void Free()
    {
        if (mBoard != nullptr)
        {
            for (int i = 0; i < mBoardSize; ++i)
            {
                delete[] mBoard[i]; 
            }
            delete[] mBoard;
            mBoard = nullptr; 
        }
    }


    const int Get_CurTurnColor() { return mTurnIndex; }

    int Update_TurnIndex(INT16 row, INT16 col)
    {
        if (nullptr == mBoard)
            return -1;

        mTurnIndex = 3 - mTurnIndex;

        mBoard[row][col] = mTurnIndex;  // 바둑돌 갱신
        return mTurnIndex;
    }

    ERROR_CODE CheckGamePlay(INT32 roomIndex, int TurnIndex, INT16 BoardSize, RoomManager* pRoomMgr);
    ERROR_CODE CheckPutStone(UINT32 clientIndex, INT16 row, INT16 col , INT16 RoomIndex ,RoomManager* pRoomMgr);

    bool CheckWin(int row, int col, int stoneColor);

    void GameEnd()
    {
        if (mBoard == nullptr)
            return;

        for (int i = 0; i < mBoardSize; ++i)
        {
            memset(mBoard[i], 0, sizeof(INT16) * mBoardSize); 
        }

        //mBoardSize = 0;   // 이건 안해줘도된다.
        //mTurnIndex = 0;
    }
 

private:
    int mMaxRooms = 0;

    int mTurnIndex = 0;
    INT16 mBoardSize = 0;
    int** mBoard = nullptr;
};