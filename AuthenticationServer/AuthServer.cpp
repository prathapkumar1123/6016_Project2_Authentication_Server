#include "AuthServer.h"
#include "bcrypt.h"
#include "openssl/sha.h"
#include "openssl/evp.h"

#include <WS2tcpip.h>
#include <iostream>
#include <random>
#include <iomanip>
#include <chrono>
#include <sstream>

std::random_device rd;
std::mt19937 generator(rd());

AuthServer::AuthServer() {
	ConnectToDatabase();

	WSAStartUp();
	InitializeSocket();
}

AuthServer::~AuthServer() {

}

void AuthServer::ConnectToDatabase() {
	printf("Connecting to database... ");
	database = new Database();
	database->ConnectToDatabase(
		DB_HOST, DB_USERNAME, DB_PASSWORD, DB_SCHEMA
	);
	printf("Done");
}

std::string AuthServer::RecordLogin(const auth::LoginRequest& accountDetails) {
	auto now = std::chrono::system_clock::now();
	std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	// Declare a tm structure to store the local time
	std::tm localTime;

	// Use localtime_s instead of localtime
	#ifdef _MSC_VER
		localtime_s(&localTime, &currentTime);
	#else
		localtime_r(&currentTime, &localTime);
	#endif

	std::stringstream ss;
	ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
	std::string dateTimeString = ss.str();

	sql::PreparedStatement* pStmt; 

	std::string selectQuery = "SELECT * FROM user where email = ?";
	pStmt = database->PrepareStatement(selectQuery.c_str());
	pStmt->setString(1, accountDetails.email());

	sql::ResultSet* resultSet = pStmt->executeQuery();

	resultSet = pStmt->executeQuery();
	
	int result = -1;

	if (resultSet->next()) {
		std::string updateQuery = "UPDATE user SET last_login = ? WHERE email = ?";
		pStmt = database->PrepareStatement(updateQuery.c_str());
		pStmt->setString(1, dateTimeString);
		pStmt->setString(2, accountDetails.email());

		dateTimeString = resultSet->getString("creation_date");

		result = pStmt->execute();
	}
	else {
		std::string query = "INSERT INTO user (email, last_login, creation_date) VALUES (?, ?, ?)";
		sql::PreparedStatement* pStmt = database->PrepareStatement(query.c_str());
		pStmt->setString(1, accountDetails.email());
		pStmt->setString(2, dateTimeString);
		pStmt->setString(3, dateTimeString);

		result = pStmt->execute();
	}

	return dateTimeString;
}

auth::RegisterResponse AuthServer::CreateAccount(const auth::RegisterRequest& accountDetails) {
	auth::RegisterResponse response;
	std::string email = accountDetails.email();
	std::string password = accountDetails.password();

	std::string salt = GenerateRandomSalt(64);
	std::string passwordHash = CreatePasswordHash(salt, password);

	sql::PreparedStatement* pStmt = database->PrepareStatement("SELECT * FROM web_auth WHERE email = ?;");
	pStmt->setString(1, email);
	sql::ResultSet* resultSet = pStmt->executeQuery();

	resultSet = pStmt->executeQuery();

	try {

		if (resultSet->next()) {
			response.set_email(email);
			response.set_requestorid(accountDetails.requestorid());
			response.set_password(accountDetails.password());
			response.set_reason(auth::RegisterResponse::ACCOUNT_ALREADY_EXISTS);
		}
		else {
			pStmt = database->PrepareStatement("INSERT INTO web_auth (email, salt, hashed_password, userId) VALUES (?, ?, ?, ?);");
			pStmt->setString(1, email);
			pStmt->setString(2, salt);
			pStmt->setString(3, passwordHash);
			pStmt->setInt(4, accountDetails.requestorid());

			int result = pStmt->execute();
			delete pStmt;

			if (result == 0) {
				response.set_email(email);
				response.set_requestorid(accountDetails.requestorid());
				response.set_password(passwordHash);
			}
			else {
				response.set_email(email);
				response.set_requestorid(accountDetails.requestorid());
				response.set_reason(auth::RegisterResponse::ACCOUNT_ALREADY_EXISTS);
			}
		}
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQL error: " << e.what() << std::endl;
		response.set_requestorid(accountDetails.requestorid());
		response.set_email(email);
		response.set_reason(auth::RegisterResponse::INTERNAL_SERVER_ERROR);
	}

	return response;
}

auth::LoginResponse AuthServer::Authenticate(const auth::LoginRequest& userDetails) {
	auth::LoginResponse response;

	std::string email = userDetails.email();
	std::string password = userDetails.password();

	try {
		sql::PreparedStatement* pStmt = database->PrepareStatement("SELECT * FROM web_auth WHERE email = ?;");
		pStmt->setString(1, email);
		sql::ResultSet* resultSet = pStmt->executeQuery();

		if (resultSet->next()) {
			// Retrieve the email from the result set
			std::string retrievedEmail = resultSet->getString("email");
			std::string retrievedSalt = resultSet->getString("salt");
			std::string retrievedPass = resultSet->getString("hashed_password");

			int userId = resultSet->getInt("userId");

			pStmt = database->PrepareStatement("SELECT * FROM user WHERE email = ?;");
			pStmt->setString(1, email);
			resultSet = pStmt->executeQuery();

			std::string passwordHash = CreatePasswordHash(retrievedSalt, password);

			if (retrievedPass == passwordHash) {
				response.set_requestorid(userId);
				std::string creationTime = RecordLogin(userDetails);

				response.set_requestorid(userDetails.requestorid());
				response.set_email(email);
				response.set_password(userDetails.password());
				response.set_message("Authentication successful, account created on " + creationTime);
			}
			else {
				response.set_requestorid(userDetails.requestorid());
				response.set_email(email);
				response.set_password(userDetails.password());
				response.set_reason(auth::LoginResponse::INVALID_CREDENTIALS);
			}
		}
		else {
			response.set_requestorid(userDetails.requestorid());
			response.set_email(email);
			response.set_password(userDetails.password());
			response.set_reason(auth::LoginResponse::INVALID_CREDENTIALS);
		}

		// Remember to free resources
		delete resultSet;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQL error: " << e.what() << std::endl;
		response.set_requestorid(userDetails.requestorid());
		response.set_email(email);
		response.set_password(userDetails.password());
		response.set_reason(auth::LoginResponse::INTERNAL_SERVER_ERROR);
	}

	return response;
}

std::string AuthServer::GenerateRandomSalt(int length) {
	// Characters to include in the salt
	const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	std::uniform_int_distribution<int> distribution(0, charset.size() - 1);

	// Generate the random salt
	std::string salt;
	for (int i = 0; i < length; ++i) {
		salt += charset[distribution(generator)];
	}

	return salt;
}

std::string AuthServer::CreatePasswordHash(std::string salt, std::string plainPassword) {

	EVP_MD_CTX* mdctx;
	const EVP_MD* md = EVP_sha256();

	mdctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(mdctx, md, NULL);

	EVP_DigestUpdate(mdctx, plainPassword.c_str(), plainPassword.length());
	EVP_DigestUpdate(mdctx, salt.c_str(), salt.length());

	unsigned char hashedPassword[SHA256_DIGEST_LENGTH];
	EVP_DigestFinal_ex(mdctx, hashedPassword, NULL);
	EVP_MD_CTX_free(mdctx);

	std::string passwordHash;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
		char buf[3];
		sprintf_s(buf, "%02x", hashedPassword[i]);
		passwordHash += buf;
	}

	return passwordHash;
}

int AuthServer::WSAStartUp() {
	printf("\nInitializing Server... ");
	WSADATA wsData;
	result = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (result != 0) {
		printf("WSAStartup failed with error %d\n", result);
		return result;
	}

	ZeroMemory(&hints, sizeof(hints));	// ensure we don't have garbage data 
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Stream
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo(LOCAL_HOST_ADDR, DEFAULT_AUTH_SERVER_PORT, &hints, &info);
	if (result != 0) {
		HandleError("GetAddrInfo", true);
		return result;
	}

	printf("Done\n");
	return result;
}

int AuthServer::InitializeSocket() {

	// Socket
	listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		HandleError("Listen Socket", true);
		return result;
	}

	// Bind
	result = bind(listenSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == SOCKET_ERROR) {
		HandleError("Bind Socket", true);
		return result;
	}

	// Listen
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		HandleError("Listen Socket", true);
		return result;
	}

	return result;
}

void AuthServer::RunSocket() {
	// Initialize active sockets and sockets ready for reading
	FD_SET activeSockets;
	FD_SET socketsReadyForReading;

	FD_ZERO(&activeSockets);
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (isRunning)
	{
		// Reset the socketsReadyForReading
		FD_ZERO(&socketsReadyForReading);
		FD_SET(listenSocket, &socketsReadyForReading);

		for (int i = 0; i < clients.size(); i++) {
			FD_SET(clients[i], &socketsReadyForReading);
		}

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		for (int i = 0; i < clients.size(); i++)
		{
			SOCKET socket = clients[i];
			if (FD_ISSET(socket, &socketsReadyForReading))
			{
				// Handle receiving data with a recv call
				//char buffer[bufSize];
				const int bufSize = 512;
				Buffer buffer(bufSize);

				int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);
				if (result == SOCKET_ERROR) {
					printf("recv failed with error %d\n", WSAGetLastError());
					CloseSocketConnection();
					break;
				}

				if (result > 0) {

					uint32_t packetSize = buffer.ReadUInt32LE();
					uint32_t messageType = buffer.ReadUInt32LE();
					uint32_t messageLength = buffer.ReadUInt32LE();
					uint32_t nameLength = buffer.ReadUInt32LE();

					std::string msg = buffer.ReadString(messageLength);
					std::string name = buffer.ReadString(nameLength);

					if (messageType == LOGIN) {
						std::cout << "Received login request from chat server." << std::endl;
						auth::LoginRequest request;
						bool success = request.ParseFromString(msg);

						if (success) {
							auth::LoginResponse result = Authenticate(request);
							std::string resMessage = result.SerializeAsString();

							SendResponseMessage(socket, resMessage, "", LOGIN_RESPONSE);
						}
						else {
							std::cout << "Failed to parse the request" << std::endl;
						}
					}
					else if (messageType == REGISTER) {
						std::cout << "Received register request from chat server." << std::endl;
						auth::RegisterRequest authRequest;

						bool success = authRequest.ParseFromString(msg);
						if (success) {
							auth::RegisterResponse result = CreateAccount(authRequest);
							std::string resMessage = result.SerializeAsString();

							SendResponseMessage(socket, resMessage, "", REGISTER_RESPONSE);
						}
						else {
							std::cout << "Failed to parse the request" << std::endl;
						}
					}
				}
			}
		}

		if (count == 0) {
			continue;
		}

		if (count == SOCKET_ERROR) {
			HandleError("Socket Select", false);
			continue;
		}

		if (count > 0) {

			if (FD_ISSET(listenSocket, &socketsReadyForReading)) {

				SOCKET newConnection = accept(listenSocket, NULL, NULL);

				if (newConnection != INVALID_SOCKET) {

					clients.push_back(newConnection);

					FD_SET(newConnection, &activeSockets);
					FD_CLR(listenSocket, &socketsReadyForReading);

				}
			}
		}
	}
}

void AuthServer::HandleError(std::string scenario, bool error) {
	int errorCode = WSAGetLastError();
	if (errorCode == WSAEWOULDBLOCK) {
		// No data available right now
	}
	else if (errorCode == WSANOTINITIALISED) {
		// WSA not yet initialized.
		std::cout << scenario << "WSA not yet initialized." << errorCode << std::endl;
	}
	else {
		// Other errors here.
		std::cout << scenario << " failed. Error - " << errorCode << std::endl;

		// Close the socket if necessary.
		if (error) {
			CloseSocketConnection();
		}
	}
}

void AuthServer::CloseSocketConnection() {
	freeaddrinfo(info);
	closesocket(listenSocket);
	WSACleanup();
}

int AuthServer::SendResponseMessage(SOCKET socket, const std::string& msg, const std::string& name, MESSAGE_TYPE type) {
	ChatMessage message;
	message.message = msg;
	message.from = name;
	message.messageLength = msg.length();
	message.nameLength = name.length();
	message.header.messageType = type;
	message.header.packetSize = message.message.length() +
		message.from.length() +
		sizeof(message.messageLength) +
		sizeof(message.header.messageType) +
		sizeof(message.nameLength) +
		sizeof(message.header.packetSize);

	const int bufSize = 512;
	Buffer buffer(bufSize);

	// Write our packet to the buffer
	buffer.WriteUInt32LE(message.header.packetSize);
	buffer.WriteUInt32LE(message.header.messageType);
	buffer.WriteUInt32LE(message.messageLength);
	buffer.WriteUInt32LE(message.nameLength);
	buffer.WriteString(message.message);
	buffer.WriteString(message.from);

	result = send(socket, reinterpret_cast<const char*>(buffer.m_BufferData.data()), message.header.packetSize, 0);
	if (result == SOCKET_ERROR) {
		HandleError("Send message", false);
	}

	return result;
}

int AuthServer::ReceiveMessages() {
	const int bufSize = 512;
	Buffer buffer(bufSize);

	// Call recv function to get data
	int result = recv(listenSocket, reinterpret_cast<char*>(buffer.m_BufferData.data()), bufSize, 0);
	if (result == SOCKET_ERROR) {
		//HandleError("Receive Data", false);
		return result;
	}

	// Check and process if result contains any data.
	if (result > 0) {
		uint32_t packetSize = buffer.ReadUInt32LE();
		uint32_t messageType = buffer.ReadUInt32LE();

		if (messageType == LOGIN) {
			uint32_t messageLength = buffer.ReadUInt32LE();
			uint32_t nameLength = buffer.ReadUInt32LE();

			std::string msg = buffer.ReadString(messageLength);

			std::cout << msg << std::endl;
		}
		else if (messageType == REGISTER) {
			uint32_t messageLength = buffer.ReadUInt32LE();
			uint32_t nameLength = buffer.ReadUInt32LE();
			std::string msg = buffer.ReadString(messageLength);
			std::string name = buffer.ReadString(nameLength);

			std::cout << msg << std::endl;
		}
	}

	return result;
}