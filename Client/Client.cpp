#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include "../10/Packet.h"

#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>

#include <string>
#include <iostream>

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
            ////Client
            //LOGIN_REQUEST = 201,
            //LOGIN_RESPONSE = 202,
            //
            //ROOM_ENTER_REQUEST = 206,
            //ROOM_ENTER_RESPONSE = 207,
            //
            //ROOM_LEAVE_REQUEST = 215,
            //ROOM_LEAVE_RESPONSE = 216,
            //
            //ROOM_CHAT_REQUEST = 221,
            //ROOM_CHAT_RESPONSE = 222,
            //ROOM_CHAT_NOTIFY = 223,

        UINT16 iInput;
        std::cout << "** 패킷 목록 **\n";
        std::cout << "1. 로그인 요청\n";
        std::cout << "2. 방 입장 요청\n";
        std::cout << "3. 방 떠나기 요청\n";
        std::cout << "4. 채팅 요청\n\n";
        std::cout << "보내실 패킷 종류를 선택하세용: ";
        std::cin >> iInput;

        if (1 != iInput)
        {
            std::cout << "미구현 다시 선택 ㄱㄱ\n\n";           
            continue;
        }

        switch (iInput)
        {
        case 1:
            iInput = (UINT16)PACKET_ID::LOGIN_REQUEST;
            break;
        case 2:
            iInput = (UINT16)PACKET_ID::ROOM_ENTER_REQUEST;
            break;
        case 3:
            iInput = (UINT16)PACKET_ID::ROOM_LEAVE_REQUEST;
            break;
        case 4:
            iInput = (UINT16)PACKET_ID::ROOM_CHAT_REQUEST;
            break;
        default:
            break;
        }

        LOGIN_REQUEST_PACKET loginPacket{};
        loginPacket.PacketId = iInput;
        loginPacket.Type = 0;
        strcpy_s(loginPacket.UserID, "testID123");
        strcpy_s(loginPacket.UserPW, "0928");
        loginPacket.PacketLength = sizeof(LOGIN_REQUEST_PACKET);

        // WSABUF 구조체 준비
        WSABUF wsaBuf;
        wsaBuf.buf = (CHAR*)&loginPacket;
        wsaBuf.len = loginPacket.PacketLength;

        // OVERLAPPED 구조체
        OVERLAPPED overlapped{};
        // (필요하면 이벤트 생성 후 overlapped.hEvent에 넣을 수 있음)

        DWORD bytesSent = 0;
        DWORD flags = 0;

        int sendResult = WSASend(clientSocket, &wsaBuf, 1, &bytesSent, flags, &overlapped, NULL);

        if (sendResult == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSA_IO_PENDING)
            {
                std::cout << "WSASend is pending (async)\n";

                // 비동기 완료 이벤트 대기 (예: GetOverlappedResult or WaitForSingleObject)
                // 여기서는 간단히 바로 GetOverlappedResult 예시 (실제로는 이벤트, IOCP로 처리)
                BOOL ret = GetOverlappedResult((HANDLE)clientSocket, &overlapped, &bytesSent, TRUE);
                if (ret)
                {
                    std::cout << "Send completed, bytes sent: " << bytesSent << "\n";
                }
                else
                {
                    std::cout << "Send failed in overlapped result\n";
                }
            }
            else
            {
                std::cout << "WSASend failed: " << err << "\n";
            }
        }
        else
        {
            std::cout << "Send immediately completed, bytes sent: " << bytesSent << "\n";
        }
    }
   
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}

