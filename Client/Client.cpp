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

void RecvRegisterMsg()
{
    char recvBuffer[1024];  // 서버에서 받은 데이터 저장용
    WSABUF wsaRecvBuf;
    wsaRecvBuf.buf = recvBuffer;
    wsaRecvBuf.len = sizeof(recvBuffer);

    OVERLAPPED recvOverlapped{};
    DWORD recvBytes = 0;
    DWORD recvFlags = 0;

    int recvResult = WSARecv(clientSocket, &wsaRecvBuf, 1, &recvBytes, &recvFlags, &recvOverlapped, NULL);
    bool isRecv = false;

    if (recvResult == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err == WSA_IO_PENDING)
        {
            // 비동기 완료 대기
            BOOL result = GetOverlappedResult((HANDLE)clientSocket, &recvOverlapped, &recvBytes, TRUE);
            if (result)
                isRecv = true;
            else       
                std::cout << "GetOverlappedResult failed on recv\n";
        }
        else
            std::cout << "WSARecv failed: " << err << "\n";
    }
    else
        isRecv = true;

    if (true == isRecv)
    {
        REGISTER_RESPONSE_PACKET* response = (REGISTER_RESPONSE_PACKET*)recvBuffer;
        if (response->Result == 0)
            std::cout << "회원가입 성공!\n";
        else if (response->Result == 2)
            std::cout << "ID 중복! 회원가입 실패\n";
        else
            std::cout << "기타 회원가입 실패\n";
    }
}

void RecvLoginMsg()
{
    char recvBuffer[1024];  // 서버에서 받은 데이터 저장용
    WSABUF wsaRecvBuf;
    wsaRecvBuf.buf = recvBuffer;
    wsaRecvBuf.len = sizeof(recvBuffer);

    OVERLAPPED recvOverlapped{};
    DWORD recvBytes = 0;
    DWORD recvFlags = 0;

    int recvResult = WSARecv(clientSocket, &wsaRecvBuf, 1, &recvBytes, &recvFlags, &recvOverlapped, NULL);
    bool isRecv = false;

    if (recvResult == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err == WSA_IO_PENDING)
        {
            BOOL result = GetOverlappedResult((HANDLE)clientSocket, &recvOverlapped, &recvBytes, TRUE);
            if (result)
                isRecv = true;
            else        
                std::cout << "GetOverlappedResult failed on recv\n";
        }
        else
            std::cout << "WSARecv failed: " << err << "\n";
    }
    else
        isRecv = true;

    if (true == isRecv)
    {
        LOGIN_RESPONSE_PACKET* response = (LOGIN_RESPONSE_PACKET*)recvBuffer;
        if (response->Result == (UINT16)ERROR_CODE::NONE)
        {
            std::cout << "로그인 성공!\n";
            isLogin = true;
        }      
        else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND)
            std::cout << "아이디를 찾을수 없음\n";
        else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW)
            std::cout << "비밀번호 다름\n";
    } 
}

void RecvRoom()
{
    char recvBuffer[1024];  // 서버에서 받은 데이터 저장용
    WSABUF wsaRecvBuf;
    wsaRecvBuf.buf = recvBuffer;
    wsaRecvBuf.len = sizeof(recvBuffer);

    OVERLAPPED recvOverlapped{};
    DWORD recvBytes = 0;
    DWORD recvFlags = 0;

    int recvResult = WSARecv(clientSocket, &wsaRecvBuf, 1, &recvBytes, &recvFlags, &recvOverlapped, NULL);
    bool isRecv = false;

    if (recvResult == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err == WSA_IO_PENDING)
        {
            BOOL result = GetOverlappedResult((HANDLE)clientSocket, &recvOverlapped, &recvBytes, TRUE);
            if (result)
                isRecv = true;
            else
                std::cout << "GetOverlappedResult failed on recv\n";
        }
        else
            std::cout << "WSARecv failed: " << err << "\n";
    }
    else
        isRecv = true;

    if (true == isRecv)
    {       
        ROOM_ENTER_RESPONSE_PACKET* response = (ROOM_ENTER_RESPONSE_PACKET*)recvBuffer;
        if (response->Result == (UINT16)ERROR_CODE::NONE)
            std::cout << "방에 입장하셨습니다.\n";
        else if (response->Result == (UINT16)ERROR_CODE::ROOM_INVALID_INDEX)
            std::cout << "방 없어요\n";
        else if (response->Result == (UINT16)ERROR_CODE::ENTER_ROOM_FULL_USER)
            std::cout << "방 꽉찼어요\n";
        else if (response->Result == (UINT16)ERROR_CODE::LOGIN_USER_NOT_FOUND)
            std::cout << "로그인을 먼저 해주셔야합니다..\n";
    }
}


#define BOARD_SIZE 15
#define CELL_SIZE 40

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawBoard(HDC hdc);
void DrawStones(HDC hdc);
void HandleClick(int x, int y);

int board[BOARD_SIZE][BOARD_SIZE] = { 0 }; // 0: 없음, 1: 흑, 2: 백
int currentPlayer = 1;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
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
        BOARD_SIZE * CELL_SIZE + 40, BOARD_SIZE * CELL_SIZE + 60,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
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
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
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

    if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
        if (board[row][col] == 0) {
            board[row][col] = currentPlayer;
            currentPlayer = 3 - currentPlayer; // 1 -> 2 -> 1
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONDOWN:
        HandleClick(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawBoard(hdc);
        DrawStones(hdc);
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




int main()
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

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cout << "Failed to connect to server\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }


    while (1)
    {
        UINT16 iInput;
        std::cout << "\n** 패킷 목록 **\t";
        if (false == isLogin)
        {
            std::cout << "현재 로그인중이 아닙니다..\n\n";
        }
        else
            std::cout << "현재 로그인중 입니다!!\n\n";

        std::cout << "0. 회원가입 요청\n";
        std::cout << "1. 로그인 요청\n";
        std::cout << "2. 방 입장 요청\n";




        std::cout << "보내실 패킷 종류를 선택하세용: ";
        std::cin >> iInput;

        static REGISTER_REQUEST_PACKET RegisterPacket{};
        static LOGIN_REQUEST_PACKET loginPacket{};
        static ROOM_ENTER_REQUEST_PACKET RoomPacket{};

        switch (iInput)
        {
        case 0:
        {
            std::string ID;
            std::string Password;

            std::cout << "\n가입하실 ID를 입력하세요: ";
            std::cin >> ID;
            std::cout << "\n가입하실 Password를 입력하세요: ";
            std::cin >> Password;

            RegisterPacket.PacketId = (UINT16)PACKET_ID::REGISTER_REQUEST;       // 회원가입
            RegisterPacket.Type = 0;
            strcpy_s(RegisterPacket.UserID, ID.c_str());
            strcpy_s(RegisterPacket.UserPW, Password.c_str());
            RegisterPacket.PacketLength = sizeof(REGISTER_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RegisterPacket;
            wsaBuf.len = RegisterPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            bool isRecv = false;

            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {
                    BOOL ret = GetOverlappedResult((HANDLE)clientSocket, &overlapped, &bytesSent, TRUE);
                    if (ret)
                        isRecv = true;
                    else
                        std::cout << "DB정보를 찾을수없습니다. MySql or Redis를 켜주세요\n";
                }
                else
                    std::cout << "WSASend failed: " << err << "\n";
            }
            else
                isRecv = true;

            if (true == isRecv)
                RecvRegisterMsg();
        }
        break;
        case 1:
        {
            std::string ID;
            std::string Password;

            std::cout << "\nID를 입력하세요: ";
            std::cin >> ID;

            std::cout << "\nPassword를 입력하세요: ";
            std::cin >> Password;

            loginPacket.PacketId = (UINT16)PACKET_ID::LOGIN_REQUEST;       // 회원가입
            loginPacket.Type = 0;
            strcpy_s(loginPacket.UserID, ID.c_str());
            strcpy_s(loginPacket.UserPW, Password.c_str());
            loginPacket.PacketLength = sizeof(LOGIN_REQUEST_PACKET);

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&loginPacket;
            wsaBuf.len = loginPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            bool isRecv = false;

            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {
                    BOOL ret = GetOverlappedResult((HANDLE)clientSocket, &overlapped, &bytesSent, TRUE);
                    if (ret)
                        isRecv = true;
                    else
                        std::cout << "DB정보를 찾을수없습니다. MySql or Redis를 켜주세요\n";
                }
                else
                    std::cout << "WSASend failed: " << err << "\n";
            }
            else
                isRecv = true;

            if (true == isRecv)
                RecvLoginMsg();
        }
        break;
        case 2:
        {
            RoomPacket.PacketId = (UINT16)PACKET_ID::ROOM_ENTER_REQUEST;
            RoomPacket.Type = 0;
            RoomPacket.PacketLength = sizeof(ROOM_ENTER_REQUEST_PACKET);
            RoomPacket.RoomNumber = 0;  // 방번호 임시 

            WSABUF wsaBuf;
            wsaBuf.buf = (CHAR*)&RoomPacket;
            wsaBuf.len = RoomPacket.PacketLength;

            OVERLAPPED overlapped{};
            DWORD bytesSent = 0;
            DWORD flags = 0;

            int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);
            bool isRecv = false;

            if (sendResult == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSA_IO_PENDING)
                {
                    BOOL ret = GetOverlappedResult((HANDLE)clientSocket, &overlapped, &bytesSent, TRUE);
                    if (ret)
                        isRecv = true;
                    else
                        std::cout << "DB정보를 찾을수없습니다. MySql or Redis를 켜주세요\n";
                }
                else
                    std::cout << "WSASend failed: " << err << "\n";
            }
            else
                isRecv = true;

            if (true == isRecv)
                RecvRoom();
        }
        break;

        }
    }


    closesocket(clientSocket);
    WSACleanup();
    return 0;
}

