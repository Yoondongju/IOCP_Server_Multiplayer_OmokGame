#include "GameManager.h"

#include "RoomManager.h"
#include "Room.h"

bool GameManager::Init(int maxRooms)
{
    mMaxRooms = maxRooms;
    return true;
}


ERROR_CODE GameManager::CheckGamePlay(INT32 roomIndex, int TurnIndex, INT16 BoardSize , RoomManager* pRoomMgr)
{
    Room* pCurRoom = pRoomMgr->GetRoomByNumber(roomIndex);
    if (ERROR_CODE::STONE_GAME_NOT_PLAYING == pCurRoom->IsPlaying())
    {
        pCurRoom->Start_GamePlay();

        if (nullptr == mBoard)
        {
            mBoardSize = BoardSize;
            mTurnIndex = TurnIndex;

            mBoard = new int* [mBoardSize];
            for (int i = 0; i < mBoardSize; ++i)
            {
                mBoard[i] = new int[mBoardSize];
                memset(mBoard[i], 0, sizeof(int) * mBoardSize);
            }         
        }

        return ERROR_CODE::NONE;
    }

    return ERROR_CODE::STONE_GAME_ALREADY_PLAYING;
}

ERROR_CODE GameManager::CheckPutStone(UINT32 clientIndex, INT16 row, INT16 col, INT16 RoomIndex , RoomManager* pRoomMgr)
{
    Room* pRoom = pRoomMgr->GetRoomByNumber(RoomIndex);
    if (pRoom == nullptr)
        return ERROR_CODE::ROOM_INVALID_INDEX;
 
    // 1. �� �����ߴ�?
    auto err = pRoom->IsPlaying();
    if (err != ERROR_CODE::NONE)
        return err;

    // 1. ���� Ȯ��
    err = pRoom->IsValidPosition(row, col, mBoard, mBoardSize);
    if (err != ERROR_CODE::NONE)
        return err;

    // 2. �̹� ���� �ִ��� Ȯ��
    err = pRoom->IsStonePlaced(row, col, mBoard, mBoardSize);
    if (err != ERROR_CODE::NONE)
        return err;

    // 3. ���� Ȯ��
    err = pRoom->IsUserTurn(clientIndex); // �ʿ� �� ���� �߰�
    if (err != ERROR_CODE::NONE)
        return err;

    pRoom->Change_CurTurnClientIndex(clientIndex); 
   
    return ERROR_CODE::NONE;
}

bool GameManager::CheckWin(int row, int col, int stoneColor)
{
    int dx[4] = { 1, 0, 1, -1 };
    int dy[4] = { 0, 1, 1, 1 };

    for (int dir = 0; dir < 4; ++dir) {
        int count = 1;

        // �� ���� ������
        int nx = col + dx[dir];
        int ny = row + dy[dir];
        while (nx >= 0 && nx < 15 && ny >= 0 && ny < 15 && mBoard[ny][nx] == stoneColor) {
            count++;
            nx += dx[dir];
            ny += dy[dir];
        }

        // �ݴ� ��������
        nx = col - dx[dir];
        ny = row - dy[dir];
        while (nx >= 0 && nx < 15 && ny >= 0 && ny < 15 && mBoard[ny][nx] == stoneColor) {
            count++;
            nx -= dx[dir];
            ny -= dy[dir];
        }

        if (count >= 5)
            return true; // �¸�
    }
    return false;
}

