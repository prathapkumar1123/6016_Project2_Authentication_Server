#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <conio.h>
#include <future>
#include <sstream>

#include "Buffer.h"
#include "Message.h"

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8412"
#define LOCAL_HOST_ADDR "127.0.0.1"

SOCKET clientSocket;

struct addrinfo* info = nullptr;
struct addrinfo hints;

std::vector<std::string> roomNames;

bool isRunning = true;

// Close the socket connection and clean up resources
void closeSocketConnection() {
    freeaddrinfo(info);
    closesocket(clientSocket);
    WSACleanup();
}

// Handle errors, clean memory if needed.
void handleError(std::string scenario, bool freeMemoryOnError) {
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
        if (freeMemoryOnError) {
            closeSocketConnection();
            freeaddrinfo(info);
            WSACleanup();
        }

    }
}

// Prepare and send a chat message
int sendMessage(const std::string& msg, const std::string& name, MESSAGE_TYPE type, SOCKET socket) {
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

    int result = send(socket, reinterpret_cast<const char*>(buffer.m_BufferData.data()), message.header.packetSize, 0);
    if (result == SOCKET_ERROR) {
        handleError("Send message", false);
    }

    return result;
}


// Join a chat room
int joinRoom(std::string& name, std::string& selectedRoom, SOCKET& socket) {
    std::string roomName;
    std::istringstream ss(selectedRoom);

    while (std::getline(ss, roomName, ',')) {
        bool isDuplicate = false;
        // Check if 'roomName' is not empty
        if (!roomName.empty()) {
            for (const std::string& existingRoom : roomNames) {
                if (existingRoom == roomName) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                roomNames.push_back(roomName);
            }
        }
    }

    int selectRoomResult = sendMessage(selectedRoom, name, JOIN_ROOM, socket);
    if (selectRoomResult == SOCKET_ERROR) {
        handleError("Join Room", false);
    }

    return 0;
}

// Receive and process incoming chat messages
void receiveMessages(SOCKET socket) {
    const int bufSize = 512;
    Buffer buffer(bufSize);

    // Call recv function to get data
    int result = recv(socket, reinterpret_cast<char*>(buffer.m_BufferData.data()), bufSize, 0);
    if (result == SOCKET_ERROR) {
        handleError("Receive Data", false);
        return;
    }

    // Check and process if result contains any data.
    if (result > 0) {
        uint32_t packetSize = buffer.ReadUInt32LE();
        uint32_t messageType = buffer.ReadUInt32LE();

        if (messageType == NOTIFICATION) {
            uint32_t messageLength = buffer.ReadUInt32LE();
            uint32_t nameLength = buffer.ReadUInt32LE();

            std::string msg = buffer.ReadString(messageLength);

            std::cout << "\r";
            std::cout << msg;
            std::cout.flush();

            std::cout << "\nYou: ";
        }
        else if (messageType == TEXT) {
            uint32_t messageLength = buffer.ReadUInt32LE();
            uint32_t nameLength = buffer.ReadUInt32LE();
            std::string msg = buffer.ReadString(messageLength);
            std::string name = buffer.ReadString(nameLength);

            std::cout << "\r";
            std::cout << name << ": " << msg;
            std::cout.flush();

            std::cout << "\nYou: ";
        }
    }
}

// Send a leave room message
void sendLeaveMessage(std::string name, std::string roomName, SOCKET socket) {
    if (!roomName.empty()) {
        // Search for 'roomName' in 'roomNames'
        auto it = std::find(roomNames.begin(), roomNames.end(), roomName);

        if (it != roomNames.end()) {
            // 'roomName' exists, remove it
            roomNames.erase(it);
        }
    }

    sendMessage(roomName, name, LEAVE_ROOM, socket);
}


// Print a horizontal line as a separator
void printLine() {
    printf("\n-------------------------------------\n");
}


// Display information about available chat rooms
void displayRoomInfo() {
    printLine();
    printf("Existing Rooms: \n1. games\n2. study\n3. news");
    printLine();
}


// The main function where the program execution begins
int main() {

    printf("Intializing...\n\n");

    // Initialize WinSock
    WSADATA wsaData;
    int result;

    // Set version 2.2 with MAKEWORD(2,2)
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("\nWSAStartup failed with error %d", result);
        return 1;
    }

    struct addrinfo* info = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));      // Ensure we don't have garbage data
    hints.ai_family = AF_INET;              // IPv4
    hints.ai_socktype = SOCK_STREAM;        // Stream
    hints.ai_protocol = IPPROTO_TCP;        // TCP
    hints.ai_flags = AI_PASSIVE;

    // Getting addr info
    result = getaddrinfo(LOCAL_HOST_ADDR, DEFAULT_PORT, &hints, &info);
    if (result == SOCKET_ERROR) {
        handleError("GetAddrInfo", true);
    }

    // Socket
    clientSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        handleError("Socket initialization", true);
    }

    // Connect
    result = connect(clientSocket, info->ai_addr, static_cast<int>(info->ai_addrlen));
    if (result == SOCKET_ERROR) { // Fixed the condition here
        handleError("Socket connection", true);
    }

    printf("Connected to the server successfully!");

    displayRoomInfo();


    // Ask user for name and room information.
    std::string name;
    std::string selectedRoom;

    std::cout << "\nEnter your name: ";
    std::getline(std::cin, name);

    std::cout << "Enter an existing room name or create a new room: ";
    std::getline(std::cin, selectedRoom);

    result = joinRoom(name, selectedRoom, clientSocket);
    if (result != 0) {
        printf("\nJoining Room Failed.\n");
    }

    printf("\n\n*** Type a message and press 'Enter' to send ***");
    printf("\n*** Type 'exit' to quit, '\LR ROOM_NAME' to leave room ***\n\n");

    // Create a separate thread for receiving messages
    std::thread receiveThread([&] {
        while (isRunning) {
            receiveMessages(clientSocket);
        }
    });

    while (isRunning) {
        std::string message;
        std::cout << "You: ";
        std::getline(std::cin, message);

        if (message == "exit") {
            for (const std::string& roomName : roomNames) {
                sendLeaveMessage(name, roomName, clientSocket);
            }
            isRunning = false;
            break;
        }

        if (message.compare(0, 3, "\\LR") == 0) {
            if (roomNames.size() > 0) {
                std::string roomName = message.substr(4);
                sendLeaveMessage(name, roomName, clientSocket);

                if (roomNames.size() == 0) {
                    isRunning = false;
                    break;
                }
            }
        }
        else if (!message.empty()) {
            sendMessage(message, name, TEXT, clientSocket);
        }
    }

    if (isRunning) {
        receiveThread.join();
    }
    else {
        receiveThread.detach();
    }

    // Close
    freeaddrinfo(info);
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
