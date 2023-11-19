#pragma once

#include <stdint.h>
#include <string>

struct PacketHeader
{
	uint32_t packetSize;
	uint32_t messageType;
};

struct ChatMessage
{
	PacketHeader header;
	uint32_t messageLength;
	uint32_t nameLength;
	std::string message;
	std::string from;
};

enum MESSAGE_TYPE {
	NOTIFICATION = 1, TEXT = 2, JOIN_ROOM = 3, LEAVE_ROOM = 4
};