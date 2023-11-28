#include "ChatServer.h"

// Server code execution begins
int main(int arg, char** argv) {
	ChatServer chatServer = ChatServer();
	chatServer.CreateRooms();
	chatServer.RunSocket();

	system("Pause");
	chatServer.CloseSocketConnection();

	return 0;
}