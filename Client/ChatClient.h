#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <vector>

#include "Message.h"
#include "ConnProperties.h"

class ChatClient
{
public:
	ChatClient();
	~ChatClient();

	void setClientName(std::string clientName);
	std::string getClientName();

	int JoinRoom(std::string& name, std::string& selectedRoom);
	int SendLeaveMessage(std::string name, std::string roomName);
	int SendChatMessage(const std::string& msg, const std::string& name, MESSAGE_TYPE type);

	void RunSocket();

	void HandleError(std::string scenario, bool freeMemoryOnError);
	void CloseSocketConnection();

	void LoginClient();

	bool isLoggedIn = false;
private:
	int WSAStartUp();
	int InitializeSocket();

	int ReceiveMessages();

	void OnLoginResponse(std::string response);
	void OnRegisterResponse(std::string response);

	SOCKET clientSocket;

	int result;

	bool isRunning = true;
	bool isWaitingForResponse = false;

	struct addrinfo* info = nullptr;
	struct addrinfo hints;

	std::string clientName;

	std::vector<std::string> chatRooms;
};