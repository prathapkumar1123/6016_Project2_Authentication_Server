# Project - TCP Chat Server and Client 

This is a simple console-based chat application built in C++ using WinSock for network communication. It allows users to connect to a server, join chat rooms, and exchange messages in real-time. This README provides an overview of the project, instructions for building and running it, and usage guidelines.

## Getting Started

### Build the Project

1. Open the `Project_ChatProgram.sln` solution file from the root folder.
2. In the Solution Explorer, right-click on the Solution and select "Clean Solution."
3. Set the platform to `x64` in the toolbar of Visual Studio.
4. Set the Configuration to `Debug` or `Release` in the toolbar.
5. Right-click on the Solution again and select "Build Solution."
6. Now you can see the `Client.exe` and `Server.exe` applications built in the output folder.
7. Output folders are `x64/Debug/`/ `x64/Release/` based on Debug/Release configuration.

#### NOTE: Run the applications in order first the Authentication Server, then Chat Server before you start using the chat client applications. When you changed the configuration, please make sure to "Clean & Rebuild Solution".

#### NOTE when building Debug: The debug build does not contain the `libprotobufd.lib`, add this library to the `./Dev/libs/` folde before building the debug configuration.


If you encounter any errors, try cleaning and rebuilding the project before running it.


## User Manual
1. Launch the Authentication Server application.
2. Launch the Chat Server application.
3. Launch the client chat application.
4. Type-1 to Login or Type-2 to Register. 
5. Provide your name when prompted.
6. Enter the name of an existing chat room or create a new room.
7. To join in multiple rooms at the same time, type the room name as a comma-separated string. ex: `games,news` | `news,study,games`
8. Start typing messages and press 'Enter' to send messages to the chat room.
9. To leave a chat room, type "\LR" followed by the room name and press 'Enter'.
10. To exit the application, type "exit" and press 'Enter'.