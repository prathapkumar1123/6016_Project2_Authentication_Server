#define WIN32_LEAN_AND_MEAN

#include "ChatClient.h"

#include <iostream>
#include <string>

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

    printf("Initializing Chat Client...\n\n");

    ChatClient client = ChatClient();
    client.LoginClient();

    displayRoomInfo();

    // Ask user for name and room information.
    std::string name;
    std::string selectedRoom;

    std::cout << "\nEnter your name: ";
    std::cin >> name;

    std::cout << "Enter an existing room name or create a new room: ";
    std::cin >> selectedRoom;
    //std::getline(std::cin, selectedRoom);

    client.setClientName(name);
    client.JoinRoom(name, selectedRoom);

    printf("\n\n*** Type a message and press 'Enter' to send ***");
    printf("\n*** Type 'exit' to quit, '\LR ROOM_NAME' to leave room ***\n\n");

    client.RunSocket();

    system("Pause");
    client.CloseSocketConnection();

    return 0;
}
