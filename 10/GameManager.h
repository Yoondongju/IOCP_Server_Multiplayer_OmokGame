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
    void Clear();

    // 이미 돌이 놓여있는가 ?
    ERROR_CODE CheckGamePlay(INT32 roomIndex, int TurnIndex, RoomManager* pRoomMgr);

    ERROR_CODE CheckPutStone(UINT32 clientIndex, INT16 row, INT16 col, int** Board ,INT16 BoardSize, INT16 RoomIndex ,RoomManager* pRoomMgr);


private:
    int mNextRoomId = 0;
    int mMaxRooms = 0;
};