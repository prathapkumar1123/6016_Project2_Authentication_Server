#pragma once

#include <WinSock2.h>
#include <vector>

#include "authentication.pb.h"
#include "Database.h"
#include "Message.h"
#include "Buffer.h"
#include "ConnProperties.h"

#pragma comment(lib, "Ws2_32.lib")

enum class CommandType
{
	REGSITER = 0,
	AUTHENTICATE = 1
};

class AuthServer
{
public:
	AuthServer();
	~AuthServer();

	void RunSocket();
private:
	Database* database;

	void ConnectToDatabase();
	std::string GenerateRandomSalt(int length);
	std::string CreatePasswordHash(std::string salt, std::string plainPassword);

	int WSAStartUp();
	int InitializeSocket();

	void HandleError(std::string scenario, bool error);
	void CloseSocketConnection();

	int ReceiveMessages();
	int SendResponseMessage(SOCKET socket, const std::string& msg, const std::string& name, MESSAGE_TYPE type);

	auth::RegisterResponse CreateAccount(const auth::RegisterRequest& accountDetails);
	auth::LoginResponse Authenticate(const auth::LoginRequest& userDetais);
	std::string RecordLogin(const auth::LoginRequest& accountDetails);
	
	SOCKET listenSocket;

	int result;
	bool isRunning = true;

	struct addrinfo* info = nullptr;
	struct addrinfo hints;
	
	std::vector<SOCKET> clients;

	std::map<int, sql::PreparedStatement*> g_PreparedStatements;

	const char* DB_HOST = "127.0.0.1:3306";
	const char* DB_USERNAME = "root";
	const char* DB_PASSWORD = "root";
	const char* DB_SCHEMA = "chat_app";
};

