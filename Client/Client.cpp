#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include "../10/Packet.h"
#include "../10/ErrorCode.h"

#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>

#include <conio.h>    

#include <string>
#include <iostream>
#include <tchar.h>

#include "SoundMager.h"


SOCKET clientSocket{};
bool isLogin = false;
bool isInRoom = false;

static REGISTER_REQUEST_PACKET RegisterPacket{};
static LOGIN_REQUEST_PACKET loginPacket{};
static ROOM_ENTER_REQUEST_PACKET RoomPacket{};

static ROOM_CHAT_REQUEST_PACKET RoomChatPacket{};

static START_GAME_REQUEST_PACKET GamePacket{};

CSound_Manager* pSoundMgr;

struct USER_DATA
{
    UINT32 iTotalMatch;
    UINT32 iWin;
    UINT32 iLose;
};

USER_DATA g_MyData{};
USER_DATA g_SelectUserData{};

std::string GetTextFromEdit(HWND hEdit)
{
    TCHAR szBuffer[256];
    GetWindowText(hEdit, szBuffer, 256);

#ifdef UNICODE
    char buffer[256];
    WideCharToMultiByte(CP_ACP, 0, szBuffer, -1, buffer, 256, NULL, NULL);
    return std::string(buffer);
#else
    return std::string(szBuffer);
#endif
}




#define BOARD_SIZE 15
#define CELL_SIZE 40


WNDPROC g_OldEditProc = nullptr;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK UserInfoPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



void DrawBoard(HDC hdc);
void DrawStones(HDC hdc);
void HandleClick(int x, int y);

int board[BOARD_SIZE][BOARD_SIZE] = { 0 }; // 0: 없음, 1: 흑, 2: 백
int currentPlayer = 1;

HWND g_hButtonSignup;
HWND g_hButtonLogin;
HWND g_hButtonEnterRoom;
HWND g_hButtonLeaveRoom;
HWND g_hButtonGameStart;
HWND g_hUserList;
HWND g_hComboRoom;

HINSTANCE g_hInstance;
HWND g_hWnd;
HWND g_hUserInfoPanel;

HWND g_hEditID;
HWND g_hEditPW;
HWND g_hChatEdit = nullptr;


INT32 g_iRoomNum = 0;
INT32 g_iUserCntInRoom = 0;
char g_UserID[MAX_USER_COUNT][MAX_USER_ID_LEN + 1] = {};
wchar_t g_SelectedID[MAX_USER_ID_LEN + 1] = { 0 };

int g_SelectedRoomIndex = -1;


std::vector<std::pair<std::string ,std::string>> g_ChatMessages; // 채팅 메시지 리스트

int g_iChatScrollOffset = 0;
const int MAX_CHAT_DISPLAY_LINES = 10;
const int CHAT_LINE_HEIGHT = 20;
const int CHAT_WINDOW_WIDTH = 400;
const int CHAT_WINDOW_HEIGHT = MAX_CHAT_DISPLAY_LINES * CHAT_LINE_HEIGHT;
const int CHAT_START_X = 700;
const int CHAT_START_Y = 380;

std::string g_strMyID;
std::string g_strMyPW;





LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        char chatBuf[256] = { 0 };
        GetWindowTextA(g_hChatEdit, chatBuf, sizeof(chatBuf));

        if (strlen(chatBuf) > 0)
        {
            RoomChatPacket.PacketId = (UINT16)PACKET_ID::ROOM_CHAT_REQUEST;
            RoomChatPacket.Type = 0;
            RoomChatPacket.PacketLength = sizeof(ROOM_CHAT_REQUEST_PACKET);

            memcpy(RoomChatPacket.Message, chatBuf, sizeof(chatBuf));
      
            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RoomChatPacket;
            wsaBuf.len = RoomChatPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {

                }
                else
                {
                    MessageBox(hWnd, _T("채팅 요청 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }

            SetWindowTextA(g_hChatEdit, "");    // 칸 비움 
        }

        return 0; // 엔터 키는 여기서 처리했으니, 시스템에 넘기지 않음
    }

    return CallWindowProc(g_OldEditProc, hWnd, msg, wParam, lParam);
}

LRESULT UserInfoPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        // 배경 흰색
        HBRUSH hBackground = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcClient, hBackground);
        DeleteObject(hBackground);

        // 폰트: 맑은 고딕, 18pt
        HFONT hFont = CreateFontW(
            20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            HANGEUL_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
            L"맑은 고딕");

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(150, 40, 170));

        // 출력할 위치
        int x = 15;
        int y = 15;
        int lineHeight = 30;

        WCHAR szBuffer[256];
        wsprintf(szBuffer, L"아이디: %s", g_SelectedID);
        TextOutW(hdc, x, y, szBuffer, lstrlenW(szBuffer));
        y += lineHeight;

        wsprintf(szBuffer, L"승: %d", g_SelectUserData.iWin);
        TextOutW(hdc, x, y, szBuffer, lstrlenW(szBuffer));
        y += lineHeight;

        wsprintf(szBuffer, L"패: %d", g_SelectUserData.iLose);
        TextOutW(hdc, x, y, szBuffer, lstrlenW(szBuffer));
        y += lineHeight;

        wsprintf(szBuffer, L"총 경기: %d판", g_SelectUserData.iTotalMatch);
        TextOutW(hdc, x, y, szBuffer, lstrlenW(szBuffer));

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        EndPaint(hWnd, &ps);
    }
    return 0;

    case WM_ERASEBKGND:
        return 1; // flicker 방지용

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}


void DrawUI(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);

    if (false == isInRoom)
    {
        int windowWidth = 1200;
        int editWidth = 300;
        int centerX = windowWidth / 2;

        int labelX = centerX - editWidth / 2 - 130; // 에디트창보다 100 왼쪽
        int labelWidth = 100;
        int editID_Y = 200;
        int editPW_Y = 260;

        TextOut(hdc, labelX, editID_Y + 10, _T("아이디 입력:"), lstrlen(_T("아이디 입력:")));
        TextOut(hdc, labelX, editPW_Y + 10, _T("비밀번호 입력:"), lstrlen(_T("비밀번호 입력:")));
    }
    else
    {
        // 폰트 설정
        HFONT hFontTitle = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        HFONT hFontContent = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        HFONT hOldFont = (HFONT)SelectObject(hdc, hFontTitle);

        // 영역 기준 설정
        const int leftColX = 650;
        const int rightColX = 950;
        const int colGapY = 30;
        const int rowHeight = 28;
        int y = 50;

        // ===== [1] 내 아이디 / 전적 =====
        SetTextColor(hdc, RGB(0, 120, 215)); // 파란색
        DrawTextA(hdc, "내 아이디", -1, new RECT{ leftColX, y, leftColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        DrawTextA(hdc, "내 전적", -1, new RECT{ rightColX, y, rightColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        y += rowHeight + 5;

        SelectObject(hdc, hFontContent);
        SetTextColor(hdc, RGB(0, 0, 0));
        DrawTextA(hdc, g_strMyID.c_str(), -1, new RECT{ leftColX + 20, y, leftColX + 250, y + rowHeight }, DT_LEFT | DT_SINGLELINE);

        char szStat[64];
        sprintf_s(szStat, "총 %d판 | 승 %d | 패 %d", g_MyData.iTotalMatch, g_MyData.iWin, g_MyData.iLose);
        DrawTextA(hdc, szStat, -1, new RECT{ rightColX + 20, y, rightColX + 250, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        y += colGapY;

        // ===== [2] 방 정보 =====
        SelectObject(hdc, hFontTitle);
        SetTextColor(hdc, RGB(255, 128, 0)); // 주황
        DrawTextA(hdc, "방 번호", -1, new RECT{ leftColX, y, leftColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        DrawTextA(hdc, "유저 수", -1, new RECT{ rightColX, y, rightColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        y += rowHeight + 5;

        SelectObject(hdc, hFontContent);
        SetTextColor(hdc, RGB(0, 0, 0));
        char szBuffer[64];
        sprintf_s(szBuffer, "%d", g_iRoomNum);
        DrawTextA(hdc, szBuffer, -1, new RECT{ leftColX + 20, y, leftColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);

        sprintf_s(szBuffer, "%d", g_iUserCntInRoom);
        DrawTextA(hdc, szBuffer, -1, new RECT{ rightColX + 20, y, rightColX + 200, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        y += colGapY;

        // ===== [3] 유저 목록 =====
        y += 10;
        SetTextColor(hdc, RGB(50, 50, 180)); // 진한 파랑
        SelectObject(hdc, hFontTitle);
        DrawTextA(hdc, "접속 유저 목록", -1, new RECT{ leftColX, y, leftColX + 300, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
        y += rowHeight;

        SelectObject(hdc, hFontContent);
        for (int i = 0; i < g_iUserCntInRoom; ++i)
        {
            sprintf_s(szBuffer, "- %s", g_UserID[i]);
            DrawTextA(hdc, szBuffer, -1, new RECT{ leftColX + 20, y, leftColX + 300, y + rowHeight }, DT_LEFT | DT_SINGLELINE);
            y += rowHeight;
        }

        // ===== [4] 채팅창 =====
        const int chatX = 650;
        const int chatW = 450;

        const int maxY = 670;
        const int marginBottom = 30;
        const int chatH = 300; 
        const int chatY = maxY - chatH - marginBottom;
        SetTextColor(hdc, RGB(128, 0, 128)); 
        SelectObject(hdc, hFontTitle);
        DrawTextA(hdc, "채팅창", -1, new RECT{ chatX, chatY - 30, chatX + 300, chatY }, DT_LEFT | DT_SINGLELINE);

        // 박스
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        Rectangle(hdc, chatX - 5, chatY - 5, chatX + chatW + 5, chatY + chatH + 5);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);

        // 메시지 출력
        SelectObject(hdc, hFontContent);
        SetTextColor(hdc, RGB(80, 0, 150));
        int totalMessages = (int)g_ChatMessages.size();

        // 화면에 그릴 수 있는 최대 줄 수 계산
        int maxChatLines = chatH / CHAT_LINE_HEIGHT;
        int startIdx = max(0, totalMessages - maxChatLines - g_iChatScrollOffset);
        int endIdx = min(totalMessages, startIdx + maxChatLines);

        int line = 0;
        for (int i = startIdx; i < endIdx; ++i)
        {
            RECT rc = {
                chatX,
                chatY + line * CHAT_LINE_HEIGHT,
                chatX + chatW,
                chatY + (line + 1) * CHAT_LINE_HEIGHT
            };
            sprintf_s(szBuffer, "%s: %s", g_ChatMessages[i].first.c_str(), g_ChatMessages[i].second.c_str());
            DrawTextA(hdc, szBuffer, -1, &rc, DT_LEFT | DT_WORDBREAK);
            line++;
        }

        // 스크롤바
        if (totalMessages > maxChatLines)
        {
            int scrollH = chatH;
            int visibleRatio = maxChatLines * 100 / totalMessages;
            int barHeight = scrollH * visibleRatio / 100;
            barHeight = max(barHeight, 10);

            int maxOffset = totalMessages - maxChatLines;
            int offsetRatio = (g_iChatScrollOffset * 100) / max(1, maxOffset);
            int barY = chatY + (scrollH * offsetRatio / 100);

            // 스크롤바 Y 위치 제한 (clamp)
            barY = max(chatY, min(barY, chatY + chatH - barHeight));

            HBRUSH hBarBrush = CreateSolidBrush(RGB(100, 100, 100));
            RECT barRect = {
                chatX + chatW + 5,
                barY,
                chatX + chatW + 10,
                barY + barHeight
            };
            FillRect(hdc, &barRect, hBarBrush);
            DeleteObject(hBarBrush);
        }

  
        SelectObject(hdc, hOldFont);
        DeleteObject(hFontTitle);
        DeleteObject(hFontContent);
    }
}


void RecvRegisterMsg(char* recvBuffer)
{
    REGISTER_RESPONSE_PACKET* response = (REGISTER_RESPONSE_PACKET*)recvBuffer;
    if (response->Result == 0)
        MessageBox(g_hWnd, _T("회원가입 성공!"), _T("성공"), MB_OK | MB_ICONMASK);
    else if (response->Result == 2)
        MessageBox(g_hWnd, _T("회원가입 실패..\n중복된 ID입니다.."), _T("실패"), MB_OK | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::USER_MGR_INVALID_USER_UNIQUEID)
        MessageBox(g_hWnd, _T("회원가입 실패..\n유효하지 않은 ID입니다.."), _T("실패"), MB_OK | MB_ICONERROR);
    else
        MessageBox(g_hWnd, _T("회원가입 실패..\n기타 이유입니다.."), _T("실패"), MB_OK | MB_ICONERROR);
}

void RecvLoginMsg(char* recvBuffer)
{
    LOGIN_RESPONSE_PACKET* response = (LOGIN_RESPONSE_PACKET*)recvBuffer;
    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        MessageBox(g_hWnd, _T("로그인 성공!"), _T("성공"), MB_OK | MB_ICONMASK);
        isLogin = true;
    }
    else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND)
        MessageBox(g_hWnd, _T("아이디를 찾을수 없습니다.."), _T("오류"), MB_OK | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW)
        MessageBox(g_hWnd, _T("비밀번호가 틀렸습니다.."), _T("오류"), MB_OK | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_ALREADY)
        MessageBox(g_hWnd, _T("이미 로그인 하셨습니다!"), _T("오류"), MB_OK | MB_ICONERROR);
}

void RecvRoom(char* recvBuffer)
{
    ROOM_ENTER_RESPONSE_PACKET* response = (ROOM_ENTER_RESPONSE_PACKET*)recvBuffer;
    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        if (false == isInRoom)
        {
            isInRoom = true;
            MessageBox(g_hWnd, _T("방에 입장하셨습니다!"), _T("성공"), MB_OK | MB_ICONMASK);
            ShowWindow(g_hEditID, SW_HIDE);
            ShowWindow(g_hEditPW, SW_HIDE);
            ShowWindow(g_hButtonSignup, SW_HIDE);
            ShowWindow(g_hButtonLogin, SW_HIDE);
            ShowWindow(g_hButtonEnterRoom, SW_HIDE);
            ShowWindow(g_hComboRoom, SW_HIDE);

            ShowWindow(g_hChatEdit, SW_SHOW);           // 채팅 입력창
            ShowWindow(g_hButtonLeaveRoom, SW_SHOW);    // 방 나가기
            ShowWindow(g_hButtonGameStart, SW_SHOW);    // 게임시작   
            ShowWindow(g_hUserList, SW_SHOW);


            g_ChatMessages.clear();
        }

     
        g_iRoomNum = response->RoomNumber;
        g_iUserCntInRoom = response->UserCntInRoom;
        memcpy(g_UserID, response->UserIDList, sizeof(response->UserIDList));


        SendMessage(g_hUserList, LB_RESETCONTENT, 0, 0);
        wchar_t wUserID[MAX_USER_COUNT][MAX_USER_ID_LEN + 1];
        for (int i = 0; i < g_iUserCntInRoom; ++i)
        {
            MultiByteToWideChar(CP_ACP, 0, g_UserID[i], -1, wUserID[i], MAX_USER_ID_LEN + 1);
            SendMessage(g_hUserList, LB_ADDSTRING, 0, (LPARAM)wUserID[i]);
        }


        InvalidateRect(g_hWnd, NULL, TRUE);

        return;
    }
    else if (response->Result == (UINT16)ERROR_CODE::ROOM_INVALID_INDEX)
        MessageBox(g_hWnd, _T("유효 하지 않은 방 번호입니다.."), _T("실패"), MB_NOFOCUS | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER)
        MessageBox(g_hWnd, _T("방이 꽉찼습니다.."), _T("실패"), MB_OK | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND)
        MessageBox(g_hWnd, _T("로그인을 먼저 해주세요.."), _T("실패"), MB_OK | MB_ICONERROR);

    g_SelectedRoomIndex = -1;   // 방 입장 실패시 초기화
}

void RecvLeaveRoom(char* recvBuffer)
{
    ROOM_LEAVE_RESPONSE_PACKET* response = (ROOM_LEAVE_RESPONSE_PACKET*)recvBuffer;
    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        if (true == isInRoom)
        {
            isInRoom = false;
            MessageBox(g_hWnd, _T("방에서 나가셨습니다!"), _T("안녕"), MB_OK | MB_ICONMASK);
            ShowWindow(g_hEditID, SW_SHOW);
            ShowWindow(g_hEditPW, SW_SHOW);
            ShowWindow(g_hButtonSignup, SW_SHOW);
            ShowWindow(g_hButtonLogin, SW_SHOW);
            ShowWindow(g_hButtonEnterRoom, SW_SHOW);
            ShowWindow(g_hComboRoom, SW_SHOW);

            ShowWindow(g_hChatEdit, SW_HIDE);           // 채팅 입력창
            ShowWindow(g_hButtonLeaveRoom, SW_HIDE);    // 방 나가기
            ShowWindow(g_hButtonGameStart, SW_HIDE);
            ShowWindow(g_hUserList, SW_HIDE);
            ShowWindow(g_hUserInfoPanel, SW_HIDE);

            InvalidateRect(g_hWnd, NULL, TRUE);

            g_SelectedRoomIndex = -1;   // 방 나갈시 초기화
        }
    }
}

void RecvLeave_Notify_Room(char* recvBuffer)
{
    ROOM_LEAVE_NOTIFY_PACKET* response = (ROOM_LEAVE_NOTIFY_PACKET*)recvBuffer;
    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        if (true == isInRoom)
        {
            g_iUserCntInRoom = response->UserCntInRoom;
            memcpy(g_UserID, response->UserIDList, sizeof(response->UserIDList));


            SendMessage(g_hUserList, LB_RESETCONTENT, 0, 0);
            wchar_t wUserID[MAX_USER_COUNT][MAX_USER_ID_LEN + 1];
            for (int i = 0; i < g_iUserCntInRoom; ++i)
            {
                MultiByteToWideChar(CP_ACP, 0, g_UserID[i], -1, wUserID[i], MAX_USER_ID_LEN + 1);
                SendMessage(g_hUserList, LB_ADDSTRING, 0, (LPARAM)wUserID[i]);
            }
        }

        InvalidateRect(g_hWnd, NULL, TRUE);
    }
}

void RecvChat(char* recvBuffer)
{
    ROOM_CHAT_NOTIFY_PACKET* response = (ROOM_CHAT_NOTIFY_PACKET*)recvBuffer;
 
    g_ChatMessages.emplace_back(response->UserID,response->Msg); 
    InvalidateRect(GetParent(g_hWnd), nullptr, TRUE);
}

void RecvGameStart(char* recvBuffer)
{
    START_GAME_RESPONSE_PACKET * response = (START_GAME_RESPONSE_PACKET*)recvBuffer;
    
    switch (response->Result)
    {
    case (UINT16)ERROR_CODE::NONE:
        MessageBox(g_hWnd, _T("게임을 시작합니다!"), _T("게임 시작"), MB_OK | MB_ICONMASK);
        break;
    case (UINT16)ERROR_CODE::GAME_NOT_FOUND_USER:
        MessageBox(g_hWnd, _T("상대할 유저를 찾을수 없습니다..\n상대할 유저를 먼저 선택하세요"), _T("실패"), MB_NOFOCUS | MB_ICONERROR);
        break;
    case (UINT16)ERROR_CODE::STONE_GAME_ALREADY_PLAYING:
        MessageBox(g_hWnd, _T("이미 게임이 진행중 입니다.."), _T("실패"), MB_NOFOCUS | MB_ICONERROR);
        break;
    case (UINT16)ERROR_CODE::GAME_FOUND_USER_ME:
        MessageBox(g_hWnd, _T("본인을 제외한 상대방을 선택하세요.."), _T("실패"), MB_NOFOCUS | MB_ICONERROR);
        break; 
    default:
        break;
    } 
}

void RecvStone(char* recvBuffer)
{
    PUT_STONE_RESPONSE_PACKET* response = (PUT_STONE_RESPONSE_PACKET*)recvBuffer;

    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        int row = response->row;
        int col = response->col;
        int stoneColor = response->stoneColor;  // 예: 1=흑, 2=백

        board[row][col] = stoneColor;

        pSoundMgr->PlaySound(TEXT("stone.mp3"), 1, 5.f);

        InvalidateRect(g_hWnd, NULL, TRUE);
    }
    else
    {
        TCHAR szError[128];

        switch ((ERROR_CODE)response->Result)
        {
        case ERROR_CODE::STONE_ALREADY_EXISTS:
            _tcscpy_s(szError, _T("이미 돌이 놓인 자리입니다."));
            break;
        case ERROR_CODE::STONE_NOT_YOUR_TURN:
            _tcscpy_s(szError, _T("당신의 차례가 아닙니다."));
            break;
        case ERROR_CODE::STONE_OUT_OF_BOUNDS:
            _tcscpy_s(szError, _T("잘못된 위치입니다."));
            break;
        case ERROR_CODE::STONE_GAME_NOT_PLAYING:
            _tcscpy_s(szError, _T("게임이 아직 시작되지 않았습니다."));
            break;
        case ERROR_CODE::STONE_GAME_OBSERVER:
            _tcscpy_s(szError, _T("관전자는 돌을 둘 수 없습니다."));
            break;
        default:
            _tcscpy_s(szError, _T("알 수 없는 오류입니다."));
            break;
        }
        MessageBox(g_hWnd, szError, _T("돌 놓기 실패"), MB_OK | MB_ICONERROR);
    }   
}

void Recv_Notify_Stone(char* recvBuffer)
{
    PUT_STONE_NOTIFY_PACKET* response = (PUT_STONE_NOTIFY_PACKET*)recvBuffer;

    int row = response->row;
    int col = response->col;
    int stoneColor = response->stoneColor;  // 예: 1=흑, 2=백

    board[row][col] = stoneColor;
    InvalidateRect(g_hWnd, NULL, TRUE);

    if ((INT16)ERROR_CODE::GAME_WIN == response->Result)
    {
        TCHAR szError[128];
        _tcscpy_s(szError, _T("승리!"));
        int result = MessageBox(g_hWnd, szError, _T("게임 종료!"), MB_OK | MB_ICONINFORMATION);
        if (result == IDOK)
        {
            for (int i = 0; i < BOARD_SIZE; ++i)
            {
                memset(board[i], 0, sizeof(int) * BOARD_SIZE);
            }
            InvalidateRect(g_hWnd, NULL, TRUE); // 보드 다시 그림
        }
    }
    else if ((INT16)ERROR_CODE::GAME_LOSE == response->Result)
    {
        TCHAR szError[128];
        _tcscpy_s(szError, _T("패배!"));
        int result = MessageBox(g_hWnd, szError, _T("게임 종료!"), MB_OK | MB_ICONERROR);
        if (result == IDOK)
        {
            for (int i = 0; i < BOARD_SIZE; ++i)
            {
                memset(board[i], 0, sizeof(int) * BOARD_SIZE);
            }
            InvalidateRect(g_hWnd, NULL, TRUE); // 보드 다시 그림
        }
    }
}

void Recv_MyData(char* recvBuffer)
{
    USER_DATA_PACKET* packet = reinterpret_cast<USER_DATA_PACKET*>(recvBuffer);
 
    g_MyData.iTotalMatch = packet->iTotalMatch;
    g_MyData.iWin = packet->iWinCount;
    g_MyData.iLose = packet->iLoseCount;
}

void Recv_OtherUserData(char* recvBuffer)
{
    USER_DATA_PACKET* packet = reinterpret_cast<USER_DATA_PACKET*>(recvBuffer);

    g_SelectUserData.iTotalMatch = packet->iTotalMatch;
    g_SelectUserData.iWin = packet->iWinCount;
    g_SelectUserData.iLose = packet->iLoseCount;

    ShowWindow(g_hUserInfoPanel, SW_SHOW);
    SetWindowPos(g_hUserInfoPanel, NULL, 970, 190, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    InvalidateRect(g_hUserInfoPanel, NULL, TRUE);
    UpdateWindow(g_hUserInfoPanel);
}

void RecvThreadFunc()
{
    static std::vector<char> recvBuffer;   // 누적 버퍼 (이전 데이터 보관용)
    char tempBuffer[1024];                 // 임시 수신 버퍼

    while (true)
    {
        WSABUF wsaRecvBuf;
        wsaRecvBuf.buf = tempBuffer;
        wsaRecvBuf.len = sizeof(tempBuffer);

        OVERLAPPED recvOverlapped{};
        DWORD recvBytes = 0;
        DWORD recvFlags = 0;

        int recvResult = WSARecv(clientSocket, &wsaRecvBuf, 1, &recvBytes, &recvFlags, &recvOverlapped, NULL);
        if (recvResult == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSA_IO_PENDING)
            {
                // 완료까지 기다림
                BOOL result = GetOverlappedResult((HANDLE)clientSocket, &recvOverlapped, &recvBytes, TRUE);
                if (!result || recvBytes == 0)
                {
                    std::cout << "수신 실패 또는 연결 종료됨\n";
                    break;
                }
            }
            else
            {
                std::cout << "WSARecv 실패: " << err << "\n";
                break;
            }
        }

        if (recvBytes > 0)
        {
            // 받은 데이터를 누적 버퍼에 추가
            recvBuffer.insert(recvBuffer.end(), tempBuffer, tempBuffer + recvBytes);

            // 패킷 단위로 처리
            while (true)
            {
                // 최소한 헤더 크기만큼은 있어야 패킷 파싱 가능
                if (recvBuffer.size() < sizeof(PACKET_HEADER))
                    break;

                // 헤더 포인터
                PACKET_HEADER* header = reinterpret_cast<PACKET_HEADER*>(recvBuffer.data());

               // // 헤더에 패킷 전체 크기 필드가 있다고 가정 (예: header->PacketSize)
                UINT16 packetSize = header->PacketLength; // 패킷 전체 크기(헤더+데이터)
               
                // 패킷 전체가 버퍼에 다 도착했는지 확인
                if (recvBuffer.size() < packetSize)
                    break;  // 아직 다 안 왔으니 더 받아야 함

                // 완성된 패킷 데이터 포인터
                char* packetData = recvBuffer.data();

                // 패킷 타입에 따른 처리
                switch ((PACKET_ID)header->PacketId)
                {
                case PACKET_ID::REGISTER_RESPONSE:
                    RecvRegisterMsg(packetData);
                    break;
                case PACKET_ID::LOGIN_RESPONSE:
                    RecvLoginMsg(packetData);
                    break;
                case PACKET_ID::ROOM_ENTER_RESPONSE:
                    RecvRoom(packetData);
                    break;
                case PACKET_ID::ROOM_LEAVE_RESPONSE:
                    RecvLeaveRoom(packetData);
                    break;
                case PACKET_ID::ROOM_LEAVE_NOTIFY:
                    RecvLeave_Notify_Room(packetData);
                    break;
                case PACKET_ID::ROOM_CHAT_NOTIFY:
                    RecvChat(packetData);
                    break;
                case PACKET_ID::START_GAME_RESPONSE_PACKET:
                    RecvGameStart(packetData);
                    break;
                case PACKET_ID::PUT_STONE_RESPONSE_PACKET:
                    RecvStone(packetData);
                    break;
                case PACKET_ID::PUT_STONE_NOTIFY_PACKET:
                    Recv_Notify_Stone(packetData);
                    break;
                case PACKET_ID::MY_DATA_RESPONSE:
                    Recv_MyData(packetData);
                    break;
                case PACKET_ID::OTHER_USER_DATA_RESPONSE:
                    Recv_OtherUserData(packetData);
                    break;

                default:
                    break;
                }

                // 처리한 패킷만큼 버퍼에서 제거
                recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + packetSize);
                ZeroMemory(tempBuffer,sizeof(tempBuffer));
            }
        }

    }
}

void DrawBoard(HDC hdc)
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        MoveToEx(hdc, CELL_SIZE, CELL_SIZE * (i + 1), NULL);
        LineTo(hdc, CELL_SIZE * BOARD_SIZE, CELL_SIZE * (i + 1));

        MoveToEx(hdc, CELL_SIZE * (i + 1), CELL_SIZE, NULL);
        LineTo(hdc, CELL_SIZE * (i + 1), CELL_SIZE * BOARD_SIZE);
    }
}

void DrawStones(HDC hdc)
{
    for (int y = 0; y < BOARD_SIZE; y++)
    {
        for (int x = 0; x < BOARD_SIZE; x++)
        {
            if (board[y][x] != 0 && board[y][x] != -1) {
                int cx = (x + 1) * CELL_SIZE;
                int cy = (y + 1) * CELL_SIZE;
                HBRUSH brush = CreateSolidBrush(board[y][x] == 1 ? RGB(0, 0, 0) : RGB(255, 255, 255));
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
                Ellipse(hdc, cx - 15, cy - 15, cx + 15, cy + 15);
                SelectObject(hdc, oldBrush);
                DeleteObject(brush);
            }
        }
    }
}

void HandleClick(int x, int y)
{
    int col = (x - CELL_SIZE / 2) / CELL_SIZE;
    int row = (y - CELL_SIZE / 2) / CELL_SIZE;

    if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE)
    {
        // 클릭한 위치를 서버로 전송
        PUT_STONE_REQUEST_PACKET packet{};

        packet.PacketId = (UINT16)PACKET_ID::PUT_STONE_REQUEST_PACKET;
        packet.Type = 0;
        packet.row = row;
        packet.col = col;
        packet.roomIndex = g_SelectedRoomIndex;
        
        packet.PacketLength = sizeof(PUT_STONE_REQUEST_PACKET);

        WSABUF wsaBuf;
        wsaBuf.buf = (CHAR*)&packet;
        wsaBuf.len = packet.PacketLength;

        OVERLAPPED overlapped{};
        DWORD bytesSent = 0;
        DWORD flags = 0;

        int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
        if (sendResult == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSA_IO_PENDING)
            {

            }
            else
            {
                MessageBox(g_hWnd, _T("돌 놓기 실패"), _T("오류"), MB_OK | MB_ICONERROR);
            }
        }
    }
}





int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        std::cout << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(11021);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    //inet_pton(AF_INET, "182.209.135.148", &serverAddr.sin_addr);    // 외부 공인 IP주소 
    


    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Failed to connect to server\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }


    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("GomokuWindowClass");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);




    WNDCLASSW wcUserInfo = { 0 };
    wcUserInfo.lpfnWndProc = UserInfoPanelProc;  
    wcUserInfo.hInstance = g_hInstance;
    wcUserInfo.lpszClassName = L"UserInfoPanel";
    wcUserInfo.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // 배경 없음
    RegisterClassW(&wcUserInfo);




    HWND hWnd = CreateWindow(
        wc.lpszClassName, _T("오목 게임 - Win32 GUI"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1360,760,
        NULL, NULL, hInstance, NULL);

    g_hWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);


    std::thread recvThread(RecvThreadFunc);     // 서버로 부터 수신을 받는 스레드
    recvThread.detach();

    MSG msg = { 0 };

    pSoundMgr = new CSound_Manager;
    pSoundMgr->Initialize();
    pSoundMgr->PlayBGM(TEXT("bgm.mp3"), 0.3f);

  

    while (GetMessage(&msg, NULL, 0, 0))
    {     
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    closesocket(clientSocket);
    WSACleanup();


    pSoundMgr->Free();
    //delete pSoundMgr;

    return (int)msg.wParam;
}

// 관전자도 돌을 둘수잇는 버그 수정해야댐
// 방에 나갓다 들어오면 채팅창 초기화 시켜야함


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) 
    {
    case WM_CREATE:
    {
        HFONT hFont = CreateFont(
            24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        // 가운데 정렬을 위한 계산
        int windowWidth = 1200;
        int editWidth = 300;
        int editHeight = 40;
        int buttonWidth = 250;
        int buttonHeight = 50;

        int centerX = windowWidth / 2;



        g_hEditID = CreateWindow(_T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            centerX - editWidth / 2, 200, editWidth, editHeight, hWnd, (HMENU)99, g_hInstance, NULL);

        g_hEditPW = CreateWindow(_T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_LEFT,
            centerX - editWidth / 2, 260, editWidth, editHeight, hWnd, (HMENU)100, g_hInstance, NULL);

        // 버튼들 (가로 가운데, 세로로 간격 두고 배치)
        g_hButtonSignup = CreateWindow(_T("BUTTON"), _T("회원가입 요청"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            centerX - buttonWidth / 2, 340, buttonWidth, buttonHeight, hWnd, (HMENU)0, g_hInstance, NULL);

        g_hButtonLogin = CreateWindow(_T("BUTTON"), _T("로그인 요청"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            centerX - buttonWidth / 2, 410, buttonWidth, buttonHeight, hWnd, (HMENU)1, g_hInstance, NULL);

        g_hButtonEnterRoom = CreateWindow(_T("BUTTON"), _T("방 입장 요청"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            centerX - buttonWidth / 2, 480, buttonWidth, buttonHeight, hWnd, (HMENU)2, g_hInstance, NULL);

        g_hComboRoom = CreateWindow(_T("COMBOBOX"), NULL,
            CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
            (centerX - buttonWidth / 2) + buttonWidth + 10  , 480, buttonWidth, 200, hWnd, (HMENU)200, g_hInstance, NULL);

        // 콤보박스에 항목 추가
        SendMessage(g_hComboRoom, CB_ADDSTRING, 0, (LPARAM)_T("방 0번"));
        SendMessage(g_hComboRoom, CB_ADDSTRING, 0, (LPARAM)_T("방 1번"));
        SendMessage(g_hComboRoom, CB_ADDSTRING, 0, (LPARAM)_T("방 2번"));
        SendMessage(g_hComboRoom, CB_ADDSTRING, 0, (LPARAM)_T("방 3번"));
        SendMessage(g_hComboRoom, CB_ADDSTRING, 0, (LPARAM)_T("방 4번"));
        SendMessage(g_hComboRoom, CB_SETCURSEL, 0, 0);
        


        g_hButtonLeaveRoom = CreateWindow(_T("BUTTON"), _T("방 나가기"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            370, 620, 100, 25, hWnd, (HMENU)3, g_hInstance, NULL);
        ShowWindow(g_hButtonLeaveRoom, SW_HIDE);

        g_hButtonGameStart = CreateWindow(_T("BUTTON"), _T("게임 시작"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            500, 620, 100, 25, hWnd, (HMENU)4, g_hInstance, NULL);
        ShowWindow(g_hButtonGameStart, SW_HIDE);


        g_hUserList = CreateWindow(_T("LISTBOX"), NULL,
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_BORDER | WS_VSCROLL,
            800, 190, 150, 100, hWnd, (HMENU)5, g_hInstance, NULL);
        ShowWindow(g_hUserList, SW_HIDE);



        g_hUserInfoPanel = CreateWindowW(L"UserInfoPanel", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            0, 0, 300, 140, hWnd, NULL, g_hInstance, NULL);
        ShowWindow(g_hUserInfoPanel, SW_HIDE);






        // 폰트 적용
        SendMessage(g_hEditID, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hEditPW, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonSignup, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonLogin, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonEnterRoom, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonLeaveRoom, WM_SETFONT, (WPARAM)hFont, TRUE);

  

        g_hChatEdit = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            650, 660, 450, 25,
            hWnd, (HMENU)1001, g_hInstance, nullptr);
        g_OldEditProc = (WNDPROC)SetWindowLongPtr(g_hChatEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        ShowWindow(g_hChatEdit, SW_HIDE);

        InvalidateRect(hWnd, NULL, TRUE); // WM_PAINT 강제 호출

        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case 0: // 회원가입 요청
        {
            std::string ID = GetTextFromEdit(g_hEditID);
            std::string PW = GetTextFromEdit(g_hEditPW);

            RegisterPacket.PacketId = (UINT16)PACKET_ID::REGISTER_REQUEST;
            RegisterPacket.Type = 0;
            strcpy_s(RegisterPacket.UserID, ID.c_str());
            strcpy_s(RegisterPacket.UserPW, PW.c_str());
            RegisterPacket.PacketLength = sizeof(REGISTER_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RegisterPacket;
            wsaBuf.len = RegisterPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {

                }
                else
                {
                    MessageBox(hWnd, _T("패킷 전송 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

        case 1: // 로그인 요청
        {
            g_strMyID = GetTextFromEdit(g_hEditID);
            g_strMyPW = GetTextFromEdit(g_hEditPW);

        
            loginPacket.PacketId = (UINT16)PACKET_ID::LOGIN_REQUEST;
            loginPacket.Type = 0;
            strcpy_s(loginPacket.UserID, g_strMyID.c_str());
            strcpy_s(loginPacket.UserPW, g_strMyPW.c_str());
            loginPacket.PacketLength = sizeof(LOGIN_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&loginPacket;
            wsaBuf.len = loginPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {

                }
                else
                {
                    MessageBox(hWnd, _T("로그인 요청 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

        case 2: // 방 입장 요청
        {
            if (!isLogin)
            {
                MessageBox(hWnd, _T("먼저 로그인해주세요!"), _T("경고"), MB_OK | MB_ICONWARNING);
                break;
            }

            g_SelectedRoomIndex = SendMessage(g_hComboRoom, CB_GETCURSEL, 0, 0);
            if (g_SelectedRoomIndex == CB_ERR)
            {
                MessageBox(hWnd, _T("방을 선택해주세요."), _T("오류"), MB_OK | MB_ICONERROR);
                break;
            }


            RoomPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_REQUEST;
            RoomPacket.Type = 0;
            RoomPacket.RoomNumber = g_SelectedRoomIndex;
            RoomPacket.PacketLength = sizeof(ROOM_ENTER_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RoomPacket;
            wsaBuf.len = RoomPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {
        
                }
                else
                {
                    MessageBox(hWnd, _T("방 입장 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

        case 3:  // 방 나가기 요청
        {
            RoomPacket.PacketId = (UINT16)PACKET_ID::ROOM_LEAVE_REQUEST;
            RoomPacket.Type = 0;
            RoomPacket.RoomNumber = g_SelectedRoomIndex;
            RoomPacket.PacketLength = sizeof(ROOM_LEAVE_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RoomPacket;
            wsaBuf.len = RoomPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {

                }
                else
                {
                    MessageBox(hWnd, _T("방 나가기 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

        case 4:  // 게임 시작 요정
        {
            GamePacket.PacketId = (UINT16)PACKET_ID::START_GAME_REQUEST_PACKET;
            GamePacket.Type = 0;
            GamePacket.roomIndex = g_SelectedRoomIndex;
            GamePacket.TurnIndex = currentPlayer;
            GamePacket.BoardSize = BOARD_SIZE;
            GamePacket.PacketLength = sizeof(START_GAME_REQUEST_PACKET);
            WideCharToMultiByte(CP_ACP, 0, g_SelectedID, -1, GamePacket.userId, sizeof(GamePacket.userId), NULL, NULL);
             
            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&GamePacket;
            wsaBuf.len = GamePacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {

                }
                else
                {
                    MessageBox(hWnd, _T("게임 시작 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

        case 5:
        {
            int index = (int)SendMessage(g_hUserList, LB_GETCURSEL, 0, 0);
            SendMessage(g_hUserList, LB_GETTEXT, index, (LPARAM)g_SelectedID);

            if (index != LB_ERR)
            {
                USER_DATA_REQUEST_PACKET packet{};

                packet.PacketId = (UINT16)PACKET_ID::OTHER_USER_DATA_REQUEST;
                packet.PacketLength = sizeof(USER_DATA_REQUEST_PACKET);
                packet.Type = 0;
                WideCharToMultiByte(CP_ACP, 0, g_SelectedID, -1, packet.userId, sizeof(packet.userId), NULL, NULL);
                
                WSABUF wsaBuf;
                wsaBuf.buf = (CHAR*)&packet;
                wsaBuf.len = packet.PacketLength;

                OVERLAPPED overlapped{};
                DWORD bytesSent = 0;
                DWORD flags = 0;

                int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
                if (sendResult == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSA_IO_PENDING)
                    {

                    }
                    else
                    {
                        MessageBox(hWnd, _T("유저 정보 요청 실패"), _T("오류"), MB_OK | MB_ICONERROR);
                    }
                }
            }
        }
            break;

        case 1001:
            break;
        }
    }
    break;


   case WM_MOUSEWHEEL:
   {
       int delta = GET_WHEEL_DELTA_WPARAM(wParam);

       int maxOffset = max(0, (int)g_ChatMessages.size() - MAX_CHAT_DISPLAY_LINES);

       if (delta > 0 && g_iChatScrollOffset < maxOffset)
       {
           g_iChatScrollOffset++;
           InvalidateRect(hWnd, nullptr, TRUE);
       }
       else if (delta < 0 && g_iChatScrollOffset > 0)
       {
           g_iChatScrollOffset--;
           InvalidateRect(hWnd, nullptr, TRUE);
       }
       return 0;
   }

 
    case WM_LBUTTONDOWN:
    {
        if (isInRoom)
        {
            HandleClick(LOWORD(lParam), HIWORD(lParam));

            POINT ptMouse{};
            GetCursorPos(&ptMouse);

            RECT rcListBox{}, rcInfoPanel{};
            GetWindowRect(g_hUserList, &rcListBox);
            GetWindowRect(g_hUserInfoPanel, &rcInfoPanel);
            if (!PtInRect(&rcListBox, ptMouse) && !PtInRect(&rcInfoPanel, ptMouse))
            {
                ShowWindow(g_hUserInfoPanel, SW_HIDE);
                SendMessage(g_hUserList, LB_SETCURSEL, (WPARAM)LB_ERR, 0);
            }

            InvalidateRect(hWnd, NULL, TRUE);
        }
    } 
        break;

    case WM_PAINT: 
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (isInRoom)
        {
            DrawBoard(hdc);
            DrawStones(hdc);
        }

        DrawUI(hdc);

        EndPaint(hWnd, &ps);
    } break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


