#include "ChatServer.h"
#include "authentication.pb.h"

#include <iostream>
#include <sstream>
#include <set>

// Print a horizontal line as a separator
void printLine() {
	printf("\n--------------------------------------\n");
}

ChatServer::ChatServer() {
	printf("Initializing Server... ");

	WSAStartUp();
	InitializeSocket();

}

ChatServer::~ChatServer() {

}

int ChatServer::WSAStartUp() {
	WSADATA wsData;
	// Set version 2.2 with MAKEWORD(2,2)
	result = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (result != 0) {
		printf("WSAStartup failed with error %d\n", result);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));	// ensure we don't have garbage data 
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Stream
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo(LOCAL_HOST_ADDR, DEFAULT_CHAT_SERVER_PORT, &hints, &info);
	if (result != 0) {
		HandleError("GetAddrInfo", true);
		return result;
	}

	result = getaddrinfo(LOCAL_HOST_ADDR, DEFAULT_AUTH_SERVER_PORT, &hints, &a_info);
	if (result != 0) {
		HandleError("GetAddrInfo", true);
		return result;
	}

	return result;
}

int ChatServer::InitializeSocket() {
	// Listen Socket
	listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		HandleError("Listen Socket Initialization", true);
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

	printf("Done");
	printf("\nConnecting to Authentication Server... ");

	// Auth Socket
	authSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (authSocket == INVALID_SOCKET) {
		HandleError("Auth Socket Initialization", true);
		return result;
	}

	result = connect(authSocket, a_info->ai_addr, static_cast<int>(a_info->ai_addrlen));
	if (result == SOCKET_ERROR) {
		HandleError("Socket connection", true);
		return result;
	}

	printf("Done");
	return result;
}

void ChatServer::RunSocket() {
	std::thread([this] {
		while (isRunning) {
			ReceiveAthSocketMessages();
		}
	}).detach();

	// Initialize active sockets and sockets ready for reading
	FD_SET activeSockets;
	FD_SET socketsReadyForReading;

	FD_ZERO(&activeSockets);
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (isRunning) {

		// Reset the socketsReadyForReading
		FD_ZERO(&socketsReadyForReading);
		FD_SET(listenSocket, &socketsReadyForReading);

		for (const auto& room : rooms) {
			for (int i = 0; i < room.second.clients.size(); i++) {
				FD_SET(room.second.clients[i], &socketsReadyForReading);
			}
		}

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		for (auto& room : rooms) {

			for (int i = 0; i < room.second.clients.size(); i++) {

				SOCKET socket = room.second.clients[i];

				if (FD_ISSET(socket, &socketsReadyForReading))
				{
					const int bufSize = 512;
					Buffer buffer(bufSize);

					int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

					if (result == 0) {
						auto& clients = room.second.clients;
						auto it = std::find(clients.begin(), clients.end(), socket);

						// Check if the socket was found and then erase it
						if (it != clients.end()) {
							clients.erase(it);
						}

						std::string leaveMessage = "Someone has left the room.\n";

						BroadcastMessage(leaveMessage, "Someone", NOTIFICATION, socket, rooms);

						i--;
						closesocket(socket);
						//FD_CLR(socket, &activeSockets);
						continue;
					} else if (result == SOCKET_ERROR) {
						auto it = std::find(room.second.clients.begin(), room.second.clients.end(), socket);

						// Check if the socket was found and then erase it
						if (it != room.second.clients.end()) {
							room.second.clients.erase(it);
						}
						
						std::string leaveMessage = "Someone has left the room.\n";

						BroadcastMessage(leaveMessage, "Someone", NOTIFICATION, socket, rooms);

						i--;
						closesocket(socket);
						continue;
					}
					else {
						// Get the data from buffer.
						uint32_t packetSize = buffer.ReadUInt32LE();
						uint32_t messageType = buffer.ReadUInt32LE();

						// Check data based on message type.
						if (messageType == NOTIFICATION) {
							uint32_t messageLength = buffer.ReadUInt32LE();
							uint32_t nameLength = buffer.ReadUInt32LE();

							std::string msg = buffer.ReadString(messageLength);

							printf(msg.c_str());
							printf("\n");
						}
						else if (messageType == TEXT) {
							// We know this is a ChatMessage
							uint32_t messageLength = buffer.ReadUInt32LE();
							uint32_t nameLength = buffer.ReadUInt32LE();
							std::string msg = buffer.ReadString(messageLength);
							std::string name = buffer.ReadString(nameLength);

							BroadcastMessage(msg, name, TEXT, socket, rooms);
						}
						else if (messageType == LEAVE_ROOM) {
							uint32_t messageLength = buffer.ReadUInt32LE();
							uint32_t nameLength = buffer.ReadUInt32LE();
							std::string roomName = buffer.ReadString(messageLength);
							std::string name = buffer.ReadString(nameLength);

							bool socketFoundInAnyRoom = false;

							// Broadcast a leave message to other clients in the room
							std::string leaveMessage = name + " has left the room.\n";
							printf(leaveMessage.c_str());

							BroadcastMessage(leaveMessage, name, NOTIFICATION, socket, rooms);

							// Iterate through all rooms in rooms
							for (auto& roomPair : rooms) {
								ChatRoom& chatroom = roomPair.second;
								// Check if the current room matches the roomName
								if (roomPair.first == roomName) {
									auto it = std::find(chatroom.clients.begin(), chatroom.clients.end(), socket);

									if (it != chatroom.clients.end()) {
										// Remove the socket from the current room
										chatroom.clients.erase(it);
										// Set a flag to indicate that the socket was found in the current room
										socketFoundInAnyRoom = true;
									}
								}
							}

							if (!socketFoundInAnyRoom) {
								// If the socket was not found in any room, close and clear it
								FD_CLR(socket, &socketsReadyForReading);
								closesocket(socket);  // Close the client socket
							}
						}
						else if (messageType == LOGIN) {
							OnLoginRequestReceived(buffer, static_cast<MESSAGE_TYPE>(messageType), socket);
						}
						else if (messageType == REGISTER) {
							OnRegisterRequestReceived(buffer, static_cast<MESSAGE_TYPE>(messageType), socket);
						}
					}
				}
			}
		}

		if (count == 0) {
			continue;
		}

		if (count > 0) {

			if (FD_ISSET(listenSocket, &socketsReadyForReading)) {

				SOCKET newConnection = accept(listenSocket, NULL, NULL);

				if (newConnection == INVALID_SOCKET) {
					printf("Accept failed with error: %d\n", WSAGetLastError());
				}
				else {

					const int bufSize = 512;
					Buffer buffer(bufSize);

					int result = recv(newConnection, (char*)(&buffer.m_BufferData[0]), bufSize, 0);
					if (result == SOCKET_ERROR) {
						HandleError("Recv", false);
					}
					else {

						uint32_t packetSize = buffer.ReadUInt32LE();
						uint32_t messageType = buffer.ReadUInt32LE();

						if (messageType == JOIN_ROOM) {
							JoinRoom(buffer, static_cast<MESSAGE_TYPE>(messageType), newConnection);
						}
						else if (messageType == LOGIN) {
							OnLoginRequestReceived(buffer, static_cast<MESSAGE_TYPE>(messageType), newConnection);
							rooms[""].clients.push_back(newConnection);
						}
						else if (messageType == REGISTER) {
							OnRegisterRequestReceived(buffer, static_cast<MESSAGE_TYPE>(messageType), newConnection);
							rooms[""].clients.push_back(newConnection);
						}
					}

					//activeConnections.push_back(newConnection);
					FD_SET(newConnection, &activeSockets);
					FD_CLR(listenSocket, &socketsReadyForReading);

					printf("Client connected with Socket: %d\n", (int) newConnection);
				}
			}
		}
	}
}

void ChatServer::JoinRoom(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket) {
	std::string roomChoice;
	std::string selectedRoom;

	uint32_t messageLength = buffer.ReadUInt32LE();
	uint32_t nameLength = buffer.ReadUInt32LE();
	std::string msg = buffer.ReadString(messageLength);
	std::string name = buffer.ReadString(nameLength);

	selectedRoom = msg;

	std::string joinMessage = name + " has joined the room.\n";

	printf("%s has joined the room.\n", name.c_str());

	// Split selectedRoom into individual room names based on commas
	std::vector<std::string> roomNames;
	std::string roomName;

	std::istringstream ss(selectedRoom);

	while (std::getline(ss, name, ',')) {
		roomNames.push_back(name);
	}

	// Add the new connection to each room
	for (const std::string& room : roomNames) {
		if (rooms.find(room) != rooms.end()) {
			rooms[room].clients.push_back(socket);
		}
		else {
			// Room doesn't exist, create a new room
			ChatRoom newRoom;
			newRoom.roomName = room;
			rooms[room] = newRoom;
			rooms[room].clients.push_back(socket);
		}
	}

	BroadcastMessage(joinMessage, name, NOTIFICATION, socket, rooms);
}

void ChatServer::OnLoginRequestReceived(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket) {
	uint32_t messageLength = buffer.ReadUInt32LE();
	uint32_t nameLength = buffer.ReadUInt32LE();
	std::string msg = buffer.ReadString(messageLength);
	std::string name = buffer.ReadString(nameLength);

	auth::LoginRequest request;
	request.ParseFromString(msg);
	request.set_requestorid(socket);

	std::string message;
	request.SerializeToString(&message);

	SendChatMessage(authSocket, message, name, messageType);
}

void ChatServer::OnRegisterRequestReceived(Buffer& buffer, MESSAGE_TYPE messageType, SOCKET socket) {
	uint32_t messageLength = buffer.ReadUInt32LE();
	uint32_t nameLength = buffer.ReadUInt32LE();
	std::string msg = buffer.ReadString(messageLength);
	std::string name = buffer.ReadString(nameLength);

	auth::RegisterRequest request;
	request.ParseFromString(msg);
	request.set_requestorid(socket);

	std::string message;
	request.SerializeToString(&message);

	SendChatMessage(authSocket, message, name, static_cast<MESSAGE_TYPE>(messageType));
}

// Handle errors, clean memory if needed.
void ChatServer::HandleError(std::string scenario, bool error) {
	int errorCode = WSAGetLastError();
	if (errorCode == WSAEWOULDBLOCK) {
		// No data available right now, continue the loop or do other work.
	}
	else if (errorCode == WSANOTINITIALISED) {
		// WSA not yet initialized.
	}
	else {
		// Other errors here.
		std::cout << scenario << " failed. Error - " << errorCode << std::endl;

		// Close the socket if necessary.
		if (error) {
			CloseSocketConnection();
			CleanUp();
		}
	}
}

// Broadcast message to the other connections in the room, except the sender.
void ChatServer::BroadcastMessage(const std::string& msg, const std::string& name, MESSAGE_TYPE type, SOCKET senderSocket, const std::map<std::string, ChatRoom>& rooms) {

	std::set<SOCKET> sentSockets;

	for (const auto& room : rooms) {
		for (SOCKET clientSocket : room.second.clients) {

			auto it = std::find(room.second.clients.begin(), room.second.clients.end(), senderSocket);

			if (it == room.second.clients.end()) {
				// senderSocket is not in this room's clients
				continue; // Skip broadcasting to this room
			}

			if (sentSockets.find(clientSocket) != sentSockets.end()) {
				continue;  // Skip broadcasting to this client
			}

			if (clientSocket != senderSocket) {
				// Create a ChatMessage to send
				ChatMessage message;
				message.message = msg;
				message.from = name;
				message.messageLength = msg.length();
				message.nameLength = name.length();
				message.header.messageType = type;
				message.header.packetSize = message.message.length()
					+ sizeof(message.messageLength)
					+ sizeof(message.header.messageType)
					+ sizeof(message.from)
					+ sizeof(message.header.packetSize);

				const int bufSize = 512;
				Buffer buffer(bufSize);

				// Write our packet to the buffer
				buffer.WriteUInt32LE(message.header.packetSize);
				buffer.WriteUInt32LE(message.header.messageType);
				buffer.WriteUInt32LE(message.messageLength);
				buffer.WriteUInt32LE(message.nameLength);
				buffer.WriteString(message.message);
				buffer.WriteString(message.from);

				// Send the message to the client
				int result = send(clientSocket, (const char*)(&buffer.m_BufferData[0]), message.header.packetSize, 0);
				if (result == SOCKET_ERROR) {
					printf("Failed to broadcast message to client %d\n", WSAGetLastError());
					return;
				}

				sentSockets.insert(clientSocket);
			}
		}
	}
}

// Clean up connections and addr info.
void ChatServer::CleanUp() {
	freeaddrinfo(info);
	WSACleanup();
}

void ChatServer::CloseSocketConnection() {
	closesocket(listenSocket);
	CleanUp();
}

// Create pre-defined rooms for users to enter  
void ChatServer::CreateRooms() {
	// Creating rooms
	printf("\nCreating rooms... ");

	std::string gameroom = "games";
	std::string studyroom = "study";
	std::string newsroom = "news";

	ChatRoom room1;
	room1.roomName = gameroom;

	ChatRoom room2;
	room2.roomName = studyroom;

	ChatRoom room3;
	room3.roomName = newsroom;

	rooms[gameroom] = room1;
	rooms[studyroom] = room2;
	rooms[newsroom] = room3;

	printf("Done\n");
}


int ChatServer::SendChatMessage(SOCKET socket, const std::string& msg, const std::string& name, MESSAGE_TYPE type) {
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
		return result;
	}

	return result;
}


int ChatServer::ReceiveMessages() {
	return 0;
}

int ChatServer::ReceiveAthSocketMessages() {
	const int bufSize = 512;
	Buffer buffer(bufSize);

	// Call recv function to get data
	int result = recv(authSocket, reinterpret_cast<char*>(buffer.m_BufferData.data()), bufSize, 0);
	if (result == SOCKET_ERROR) {
		HandleError("Receive Data ", false);
		return result;
	}

	// Check and process if result contains any data.
	if (result > 0) {
		uint32_t packetSize = buffer.ReadUInt32LE();
		uint32_t messageType = buffer.ReadUInt32LE();
		uint32_t messageLength = buffer.ReadUInt32LE();
		uint32_t nameLength = buffer.ReadUInt32LE();

		std::string msg = buffer.ReadString(messageLength);
		std::string name = buffer.ReadString(nameLength);

		int socketId = 0;

		if (messageType == LOGIN_RESPONSE) {
			auth::LoginResponse response;
			bool success = response.ParseFromString(msg);
			socketId = response.requestorid();
			if (success) {
				socketId = response.requestorid();
			}
		}
		else if (messageType == REGISTER_RESPONSE) {
			auth::RegisterResponse response;
			bool success = response.ParseFromString(msg);
			if (success) {
				socketId = response.requestorid();
			}
		}

		for (auto& room : rooms) {
			for (int i = 0; i < room.second.clients.size(); i++) {
				SOCKET socket = room.second.clients[i];

				if (socket == socketId) {
					SendChatMessage(socket, msg, name, static_cast<MESSAGE_TYPE>(messageType));
				}
			}
		}
	}

	return result;
}