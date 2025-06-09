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


SOCKET clientSocket{};
bool isLogin = false;
bool isInRoom = false;

static REGISTER_REQUEST_PACKET RegisterPacket{};
static LOGIN_REQUEST_PACKET loginPacket{};
static ROOM_ENTER_REQUEST_PACKET RoomPacket{};

static ROOM_CHAT_REQUEST_PACKET RoomChatPacket{};


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

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawBoard(HDC hdc);
void DrawStones(HDC hdc);
void HandleClick(int x, int y);

int board[BOARD_SIZE][BOARD_SIZE] = { 0 }; // 0: 없음, 1: 흑, 2: 백
int currentPlayer = 1;

HWND g_hButtonSignup;
HWND g_hButtonLogin;
HWND g_hButtonEnterRoom;
HWND g_hButtonLeaveRoom;

HINSTANCE g_hInstance;
HWND g_hWnd;

HWND g_hEditID;
HWND g_hEditPW;
HWND g_hChatEdit = nullptr;
WNDPROC g_OldEditProc = nullptr;
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


INT32 g_iRoomNum = 0;
INT32 g_iUserCntInRoom = 0;
char g_UserID[MAX_USER_COUNT][MAX_USER_ID_LEN + 1] = {};

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

            ShowWindow(g_hChatEdit, SW_SHOW);           // 채팅 입력창
            ShowWindow(g_hButtonLeaveRoom, SW_SHOW);    // 방 나가기

            InvalidateRect(g_hWnd, NULL, TRUE);
        }


        g_iRoomNum = response->RoomNumber;
        g_iUserCntInRoom = response->UserCntInRoom;
        memcpy(g_UserID, response->UserIDList, sizeof(response->UserIDList));
    }
    else if (response->Result == (UINT16)ERROR_CODE::ROOM_INVALID_INDEX)
        MessageBox(g_hWnd, _T("유효 하지 않은 방 번호입니다.."), _T("실패"), MB_NOFOCUS | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER)
        MessageBox(g_hWnd, _T("방이 꽉찼습니다.."), _T("실패"), MB_OK | MB_ICONERROR);
    else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND)
        MessageBox(g_hWnd, _T("로그인을 먼저 해주세요.."), _T("실패"), MB_OK | MB_ICONERROR);
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

            ShowWindow(g_hChatEdit, SW_HIDE);           // 채팅 입력창
            ShowWindow(g_hButtonLeaveRoom, SW_HIDE);    // 방 나가기

            InvalidateRect(g_hWnd, NULL, TRUE);
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
        }
    }
}

void RecvChat(char* recvBuffer)
{
    ROOM_CHAT_NOTIFY_PACKET* response = (ROOM_CHAT_NOTIFY_PACKET*)recvBuffer;
 
    g_ChatMessages.emplace_back(response->UserID,response->Msg); 
    InvalidateRect(GetParent(g_hWnd), nullptr, TRUE);
}


void RecvStone(char* recvBuffer)
{
    PUT_STONE_RESPONSE_PACKET* response = (PUT_STONE_RESPONSE_PACKET*)recvBuffer;

    if (response->Result == (UINT16)ERROR_CODE::NONE)
    {
        int row = response->row;
        int col = response->col;
        int stoneColor = response->stoneColor;  // 예: 1=흑, 2=백

        if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE)
        {
            board[row][col] = stoneColor;

           
            currentPlayer = 3 - stoneColor;     // 1,2

            InvalidateRect(g_hWnd, NULL, TRUE); 
        }
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
        default:
            _tcscpy_s(szError, _T("알 수 없는 오류입니다."));
            break;
        }

        MessageBox(g_hWnd, szError, _T("돌 놓기 실패"), MB_OK | MB_ICONERROR);
    }   
}


void RecvThreadFunc()
{
    while (true)
    {
        char recvBuffer[1024];
        WSABUF wsaRecvBuf;
        wsaRecvBuf.buf = recvBuffer;
        wsaRecvBuf.len = sizeof(recvBuffer);

        OVERLAPPED recvOverlapped{};
        DWORD recvBytes = 0;
        DWORD recvFlags = 0;

        int recvResult = WSARecv(clientSocket, &wsaRecvBuf, 1, &recvBytes, &recvFlags, &recvOverlapped, NULL);
        if (recvResult == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSA_IO_PENDING)
            {
                // 완료까지 기다린다
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
            PACKET_HEADER* header = reinterpret_cast<PACKET_HEADER*>(recvBuffer);
            switch ((PACKET_ID)header->PacketId)
            {
            case PACKET_ID::REGISTER_RESPONSE:
                RecvRegisterMsg(recvBuffer);
                break;
            case PACKET_ID::LOGIN_RESPONSE:
                RecvLoginMsg(recvBuffer);
                break;
            case PACKET_ID::ROOM_ENTER_RESPONSE:
                RecvRoom(recvBuffer);
                break;
            case PACKET_ID::ROOM_LEAVE_RESPONSE:
                RecvLeaveRoom(recvBuffer);
                break;       
            case PACKET_ID::ROOM_LEAVE_NOTIFY:
                RecvLeave_Notify_Room(recvBuffer);
                break;       
            case PACKET_ID::ROOM_CHAT_NOTIFY:
                RecvChat(recvBuffer);
                break;
            case PACKET_ID::PUT_STONE_RESPONSE_PACKET:  // 돌 놓았을때
                RecvStone(recvBuffer);
                break;


            default:              
                break;
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

void DrawStones(HDC hdc, PUT_STONE_NOTIFY_PACKET* pkt)
{
    for (int y = 0; y < BOARD_SIZE; y++)
    {
        for (int x = 0; x < BOARD_SIZE; x++)
        {
            if (board[y][x] != 0) {
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
        packet.BoardSize = BOARD_SIZE;
        packet.row = y;
        packet.col = x;

        loginPacket.PacketLength = sizeof(PUT_STONE_REQUEST_PACKET);

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
    //inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    inet_pton(AF_INET, "182.209.135.148", &serverAddr.sin_addr);    // 외부 공인 IP주소 
    


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

    HWND hWnd = CreateWindow(
        wc.lpszClassName, _T("오목 게임 - Win32 GUI"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200,720,
        NULL, NULL, hInstance, NULL);

    g_hWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);


    std::thread recvThread(RecvThreadFunc);     // 서버로 부터 수신을 받는 스레드
    recvThread.detach();

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    closesocket(clientSocket);
    WSACleanup();

    return (int)msg.wParam;
}




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

        g_hButtonLeaveRoom = CreateWindow(_T("BUTTON"), _T("방 나가기"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            900, 340, 100, 25, hWnd, (HMENU)3, g_hInstance, NULL);
        ShowWindow(g_hButtonLeaveRoom, SW_HIDE);

        // 폰트 적용
        SendMessage(g_hEditID, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hEditPW, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonSignup, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonLogin, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonEnterRoom, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hButtonLeaveRoom, WM_SETFONT, (WPARAM)hFont, TRUE);

  

        g_hChatEdit = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            700, 600, 450, 25,
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

            RoomPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_REQUEST;
            RoomPacket.Type = 0;
            RoomPacket.RoomNumber = 0;
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
            RoomPacket.RoomNumber = 0;
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

            SetTextColor(hdc, RGB(120, 180, 30));
 
            CHAR szBuffer[128] = { 0 };
            int startY = 100;
            int lineHeight = 30;
            int currentLine = 0;

            auto MakeRect = [&](int line) -> RECT {
                return { 750, startY + line * lineHeight, 1100, startY + (line + 1) * lineHeight };
            };

            RECT rt = MakeRect(currentLine++);
            // 방 번호
            sprintf_s(szBuffer, "접속중인 방 번호: %d", g_iRoomNum);
            DrawTextA(hdc, szBuffer, -1, &rt, DT_LEFT | DT_SINGLELINE);

            // 유저 수
            rt = MakeRect(currentLine++);
            sprintf_s(szBuffer, "접속중인 유저 수: %d", g_iUserCntInRoom);
            DrawTextA(hdc, szBuffer, -1, &rt, DT_LEFT | DT_SINGLELINE);

            // 유저 ID들           
            for (int i = 0; i < g_iUserCntInRoom; ++i)
            {
                rt = MakeRect(currentLine++);

                sprintf_s(szBuffer, "접속중인 유저 ID: %s", g_UserID[i]);
                DrawTextA(hdc, szBuffer, -1, &rt, DT_LEFT | DT_SINGLELINE);
            }


            

           
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            HPEN hRedPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hRedPen);

            rt = { CHAT_START_X ,CHAT_START_Y - 40,CHAT_START_X + 100,CHAT_START_Y };
            sprintf_s(szBuffer, "채팅창");
            DrawTextA(hdc, szBuffer, -1, &rt, DT_LEFT | DT_SINGLELINE);

            Rectangle(hdc, CHAT_START_X - 5, CHAT_START_Y - 10,
                CHAT_START_X + CHAT_WINDOW_WIDTH + 5,
                CHAT_START_Y + CHAT_WINDOW_HEIGHT + 5);

            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hRedPen);

            // 채팅 메시지 그리기
            SetTextColor(hdc, RGB(255, 0, 255));

            int totalMessages = (int)g_ChatMessages.size();
            int startIdx = max(0, totalMessages - MAX_CHAT_DISPLAY_LINES - g_iChatScrollOffset);
            int endIdx = min(totalMessages, startIdx + MAX_CHAT_DISPLAY_LINES);

            char szChatLine[512] = { 0 };
            int drawLine = 0;
            for (int i = startIdx; i < endIdx; ++i)
            {
                RECT chatRect = {
                    CHAT_START_X,
                    CHAT_START_Y + drawLine * CHAT_LINE_HEIGHT,
                    CHAT_START_X + CHAT_WINDOW_WIDTH,
                    CHAT_START_Y + (drawLine + 1) * CHAT_LINE_HEIGHT
                };


                sprintf_s(szChatLine, "%s 님의 채팅: %s", g_ChatMessages[i].first.c_str(), g_ChatMessages[i].second.c_str());
                DrawTextA(hdc, szChatLine, -1, &chatRect, DT_LEFT | DT_WORDBREAK);     
                drawLine++;
            }

            // 스크롤바 그리기
            if (totalMessages > MAX_CHAT_DISPLAY_LINES)
            {
                int scrollBarHeight = CHAT_WINDOW_HEIGHT;
                int visibleRatio = MAX_CHAT_DISPLAY_LINES * 100 / totalMessages;
                int barHeight = scrollBarHeight * visibleRatio / 100;
                barHeight = max(barHeight, 10);

                int maxOffset = totalMessages - MAX_CHAT_DISPLAY_LINES;
                int offsetRatio = (g_iChatScrollOffset * 100) / max(1, maxOffset);
                int barY = CHAT_START_Y + (scrollBarHeight * offsetRatio / 100);

                if (barY + barHeight > CHAT_START_Y + CHAT_WINDOW_HEIGHT)
                {
                    barY = CHAT_START_Y + CHAT_WINDOW_HEIGHT - barHeight;
                }

                HBRUSH hBarBrush = CreateSolidBrush(RGB(100, 100, 100));
                RECT barRect = {
                    CHAT_START_X + CHAT_WINDOW_WIDTH + 5,
                    barY,
                    CHAT_START_X + CHAT_WINDOW_WIDTH + 10,
                    barY + barHeight
                };
                FillRect(hdc, &barRect, hBarBrush);
                DeleteObject(hBarBrush);
            }

        }
        else
        {
            // 배경 투명 & 텍스트 색상 지정
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(210, 200, 30));

            // 기본 폰트 지정 (또는 따로 만든 hFont 선택)
            HFONT hFont = CreateFont(
                24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            int windowWidth = 1200;
            int editWidth = 300;
            int editHeight = 40;
            int centerX = windowWidth / 2;

            RECT rcID = { centerX - editWidth / 2 - 130, 200, centerX - editWidth / 2 - 20, 200 + editHeight };
            RECT rcPW = { centerX - editWidth / 2 - 130, 260, centerX - editWidth / 2 - 20, 260 + editHeight };

            DrawText(hdc, _T("아이디 입력:"), -1, &rcID, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            DrawText(hdc, _T("비밀번호 입력:"), -1, &rcPW, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
        }

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


