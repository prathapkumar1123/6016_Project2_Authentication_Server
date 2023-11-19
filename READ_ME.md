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

#### NOTE: Start the Server application before you start using the client applications.

#### NOTE: When you changed the configuration, please make sure to "Clean & Rebuild Solution".

If you encounter any errors, try cleaning and rebuilding the project before running it.



## User Manual

1. Launch the client chat application.
2. Provide your name when prompted.
3. Enter the name of an existing chat room or create a new room.
4. To join in multiple rooms at the same time, type the room name as a comma-separated string. ex: `games,news` | `news,study,games`
5. Start typing messages and press 'Enter' to send messages to the chat room.
6. To leave a chat room, type "\LR" followed by the room name and press 'Enter'.
7. To exit the application, type "exit" and press 'Enter'.