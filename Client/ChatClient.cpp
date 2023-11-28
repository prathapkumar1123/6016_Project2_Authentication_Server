#include "ChatClient.h"
#include "Buffer.h"

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "Ws2_32.lib")

#include "authentication.pb.h"

#include <iostream>
#include <sstream>
#include <future>

ChatClient::ChatClient() {
    WSAStartUp();
    InitializeSocket();
}

ChatClient::~ChatClient() {

}

int ChatClient::WSAStartUp() {
    WSADATA wsData;
    result = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (result != 0) {
        printf("\nWSAStartup failed with error %d", result);
        return result;
    }

    ZeroMemory(&hints, sizeof(hints));      // Ensure we don't have garbage data
    hints.ai_family = AF_INET;              // IPv4
    hints.ai_socktype = SOCK_STREAM;        // Stream
    hints.ai_protocol = IPPROTO_TCP;        // TCP
    hints.ai_flags = AI_PASSIVE;

    // Getting addr info
    result = getaddrinfo(LOCAL_HOST_ADDR, DEFAULT_CHAT_SERVER_PORT, &hints, &info);
    if (result == SOCKET_ERROR) {
        HandleError("GetAddrInfo", true);
        return result;
    }

    return result;
}

int ChatClient::InitializeSocket() {
    clientSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        HandleError("Socket initialization", true);
        return result;
    }

    result = connect(clientSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (result == SOCKET_ERROR) {
        HandleError("Socket connection", true);
        return result;
    }

    printf("Connected to the server successfully!\n");

    std::thread([this] {
        while (isRunning) {
            ReceiveMessages();
        }
    }).detach();

    return result;
}


// Close the socket connection and clean up resources
void ChatClient::CloseSocketConnection() {
    freeaddrinfo(info);
    closesocket(clientSocket);
    WSACleanup();
}

// Handle errors, clean memory if needed.
void ChatClient::HandleError(std::string scenario, bool error) {
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

int ChatClient::SendChatMessage(const std::string& msg, const std::string& name, MESSAGE_TYPE type) {
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

    result = send(clientSocket, reinterpret_cast<const char*>(buffer.m_BufferData.data()), message.header.packetSize, 0);
    if (result == SOCKET_ERROR) {
        HandleError("Send message", false);
    }
 
    return result;
}

int ChatClient::JoinRoom(std::string& name, std::string& selectedRoom) {
    std::string roomName;
    std::istringstream ss(selectedRoom);

    while (std::getline(ss, roomName, ',')) {
        bool isDuplicate = false;
        // Check if 'roomName' is not empty
        if (!roomName.empty()) {
            for (const std::string& existingRoom : chatRooms) {
                if (existingRoom == roomName) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                chatRooms.push_back(roomName);
            }
        }
    }

    result = SendChatMessage(selectedRoom, name, JOIN_ROOM);
    if (result == SOCKET_ERROR) {
        HandleError("Join Room", false);
    }

    printf("Joined Room Successfully!");

    return result;
}

int ChatClient::ReceiveMessages() {
    const int bufSize = 512;
    Buffer buffer(bufSize);

    // Call recv function to get data
    int result = recv(clientSocket, reinterpret_cast<char*>(buffer.m_BufferData.data()), bufSize, 0);
    if (result == SOCKET_ERROR) {
        HandleError("Receive Data", false);
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

        if (messageType == NOTIFICATION) {
            std::cout << "\r";
            std::cout << msg;
            std::cout.flush();

            std::cout << "\nYou: ";
        }
        else if (messageType == TEXT) {
            std::cout << "\r";
            std::cout << name << ": " << msg;
            std::cout.flush();

            std::cout << "\nYou: ";
        }
        else if (messageType == REGISTER_RESPONSE) {
            OnRegisterResponse(msg);
        }
        else if (messageType == LOGIN_RESPONSE) {
            OnLoginResponse(msg);
        }
    }

    return result;
}

int ChatClient::SendLeaveMessage(std::string name, std::string roomName) {
    if (!roomName.empty()) {
        // Search for 'roomName' in 'roomNames'
        auto it = std::find(chatRooms.begin(), chatRooms.end(), roomName);

        if (it != chatRooms.end()) {
            // 'roomName' exists, remove it
            chatRooms.erase(it);
        }
    }

    SendChatMessage(roomName, name, LEAVE_ROOM);
    return result;
}

void ChatClient::setClientName(std::string clientName) {
    this->clientName = clientName;
}

std::string ChatClient::getClientName() {
    return clientName;
}

void ChatClient::RunSocket() {
    while (isRunning) {
        if (isLoggedIn) {
            std::string message;
           
            std::cout << "\r";
            std::cout.flush();
            std::cout << "You: ";

            std::getline(std::cin, message);

            if (message == "exit") {
                for (const std::string& roomName : chatRooms) {
                    SendLeaveMessage(clientName, roomName);
                }
                isRunning = false;
                continue;
            }

            if (message.compare(0, 3, "\\LR") == 0) {
                if (chatRooms.size() > 0) {
                    std::string roomName = message.substr(4);
                    SendLeaveMessage(clientName, roomName);

                    if (chatRooms.size() == 0) {
                        isRunning = false;
                        continue;
                    }
                }
            }
            else if (!message.empty()) {
                SendChatMessage(message, clientName, TEXT);
            }
        }
    }
}

void ChatClient::LoginClient() {
    while (!isLoggedIn) {
        int choice = 0;
        if (!isWaitingForResponse && choice != 1 && choice != 2) {
            std::cout << "Press 1 to Login, 2 to Register: " << std::endl;
            std::cin >> choice;

            std::string email;
            std::string password;

            if (choice == 1 || choice == 2) {
                std::cout << "Enter Email: ";
                std::cin >> email;
                std::cout << "Enter Password: ";
                std::cin >> password;
            }

            if (choice == 1) {
                std::string message;
                auth::LoginRequest loginReq = auth::LoginRequest();
                loginReq.set_email(email);
                loginReq.set_password(password);
                loginReq.set_requestorid(clientSocket);
                loginReq.SerializeToString(&message);

                SendChatMessage(message, clientName, LOGIN);
                isWaitingForResponse = true;
            }

            if (choice == 2) {
                std::string message;
                auth::RegisterRequest authReq = auth::RegisterRequest();
                authReq.set_email(email);
                authReq.set_password(password);
                authReq.set_requestorid(clientSocket);
                authReq.SerializeToString(&message);

                SendChatMessage(message, clientName, REGISTER);
                isWaitingForResponse = true;
            }
        }
    }
}

void ChatClient::OnLoginResponse(std::string responseStr) {
    auth::LoginResponse response;
    response.ParseFromString(responseStr);

    if (response.has_reason()) {
        // The 'reason' field is set, so we can access its value
        switch (response.reason()) {
        case auth::LoginResponse::INVALID_CREDENTIALS:
            std::cout << "Invalid Credentials, Please try again." << std::endl;
            break;
        case auth::LoginResponse::INTERNAL_SERVER_ERROR:
        default:
            std::cout << "Problem with the server, Please try again." << std::endl;
            break;
        }

        isLoggedIn = false;
    }
    else {
        isLoggedIn = true;
        std::cout << response.message() << std::endl;
    }

    isWaitingForResponse = false;
}

void ChatClient::OnRegisterResponse(std::string responseStr) {
    auth::RegisterResponse response;
    response.ParseFromString(responseStr);

    if (response.has_reason()) {
        // The 'reason' field is set, so we can access its value
        switch (response.reason()) {
        case auth::RegisterResponse::ACCOUNT_ALREADY_EXISTS:
            std::cout << "Account already exists, Please try with different details." << std::endl;
            break;
        case auth::RegisterResponse::INTERNAL_SERVER_ERROR:
        default:
            std::cout << "Problem with the server, Please try again." << std::endl;
            break;
        }
    }
    else {
        std::cout << "Registration Successful!" << std::endl;
    }

    isLoggedIn = false;
    isWaitingForResponse = false;
}
