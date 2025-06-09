#include "GameManager.h"

#include "RoomManager.h"
#include "Room.h"

bool GameManager::Init(int maxRooms)
{
    mMaxRooms = maxRooms;
    return true;
}

void GameManager::Clear()
{

    mNextRoomId = 0;
}

ERROR_CODE GameManager::CheckGamePlay(INT32 roomIndex, int TurnIndex, RoomManager* pRoomMgr)
{
    Room* pCurRoom = pRoomMgr->GetRoomByNumber(roomIndex);
    if (ERROR_CODE::STONE_GAME_NOT_PLAYING == pCurRoom->IsPlaying())
    {
        pCurRoom->Start_GamePlay();
        return ERROR_CODE::NONE;
    }

    return ERROR_CODE::STONE_GAME_ALREADY_PLAYING;
}

ERROR_CODE GameManager::CheckPutStone(UINT32 clientIndex, INT16 row, INT16 col, int** Board ,INT16 BoardSize, INT16 RoomIndex , RoomManager* pRoomMgr)
{
    Room* pRoom = pRoomMgr->GetRoomByNumber(RoomIndex);
    if (pRoom == nullptr)
        return ERROR_CODE::ROOM_INVALID_INDEX;

    // 1. 겜 시작했니?
    auto err = pRoom->IsPlaying();
    if (err != ERROR_CODE::NONE)
        return err;

    // 1. 범위 확인
    auto err = pRoom->IsValidPosition(row, col, Board, BoardSize);
    if (err != ERROR_CODE::NONE)
        return err;

    // 2. 이미 돌이 있는지 확인
    err = pRoom->IsStonePlaced(row, col, Board, BoardSize);
    if (err != ERROR_CODE::NONE)
        return err;

    // 3. 차례 확인
    err = pRoom->IsUserTurn(clientIndex); // 필요 시 인자 추가
    if (err != ERROR_CODE::NONE)
        return err;


    pRoom->Change_CurTurnClientIndex(clientIndex);

    return ERROR_CODE::NONE;
}

