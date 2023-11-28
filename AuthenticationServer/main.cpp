#include "AuthServer.h"


void main(int arg, char** argv) {
	AuthServer server = AuthServer();
	server.RunSocket();
}