#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <vector>
#include <map>

#include "Buffer.h"
#include "Message.h"
#include "ConnProperties.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

struct ChatRoom {
	std::string roomName;
	std::vector<SOCKET> clients;
};

class ChatServer
{
public:
	ChatServer();
	~ChatServer();

	void CreateRooms();

	void RunSocket();
	void CloseSocketConnection();
private:
	SOCKET listenSocket;
	SOCKET authSocket;

	int result;

	bool isRunning = true;

	struct addrinfo* a_info = nullptr;
	struct addrinfo* info = nullptr;
	struct addrinfo hints;

	std::vector<std::string> chatRooms;
	std::map<std::string, ChatRoom> rooms;

	// Methods

	int WSAStartUp();
	int InitializeSocket();

	int ReceiveMessages();
	int ReceiveAthSocketMessages();
	int SendChatMessage(SOCKET socket, const std::string& msg, const std::string& name, MESSAGE_TYPE type);

	void BroadcastMessage(
		const std::string& msg,
		const std::string& name,
		MESSAGE_TYPE type,
		SOCKET senderSocket,
		const std::map<std::string, ChatRoom>& rooms);

	void HandleError(std::string scenario, bool freeMemoryOnError);
	void CleanUp();

	void JoinRoom(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket);
	void OnLoginRequestReceived(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket);
	void OnRegisterRequestReceived(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket);
};

