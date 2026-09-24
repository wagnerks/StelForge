#include "EditorProtocol.h"

#include <cstring>

namespace SFE::EditorProtocol {

	bool writePacket(std::vector<uint8_t>& out, MessageType type, std::string_view json, const void* binary, uint32_t binarySize)
	{
		PacketHeader header{};
		header.type = static_cast<uint32_t>(type);
		header.jsonSize = static_cast<uint32_t>(json.size());
		header.binarySize = binarySize;

		const size_t total = sizeof(PacketHeader) + header.jsonSize + header.binarySize;
		out.resize(total);
		std::memcpy(out.data(), &header, sizeof(PacketHeader));
		if (header.jsonSize > 0) {
			std::memcpy(out.data() + sizeof(PacketHeader), json.data(), header.jsonSize);
		}
		if (header.binarySize > 0 && binary) {
			std::memcpy(out.data() + sizeof(PacketHeader) + header.jsonSize, binary, header.binarySize);
		}
		return true;
	}

	bool readPacket(const uint8_t* data, size_t size, PacketHeader& header, std::string& json, std::vector<uint8_t>& binary)
	{
		if (size < sizeof(PacketHeader)) {
			return false;
		}
		std::memcpy(&header, data, sizeof(PacketHeader));
		if (header.magic != kMagic) {
			return false;
		}
		const size_t expected = sizeof(PacketHeader) + header.jsonSize + header.binarySize;
		if (size < expected) {
			return false;
		}
		json.assign(reinterpret_cast<const char*>(data + sizeof(PacketHeader)), header.jsonSize);
		binary.assign(data + sizeof(PacketHeader) + header.jsonSize, data + expected);
		return true;
	}

	const char* messageTypeName(MessageType type)
	{
		switch (type) {
		case MessageType::Ping: return "Ping";
		case MessageType::Pong: return "Pong";
		case MessageType::SubscribeFrames: return "SubscribeFrames";
		case MessageType::UnsubscribeFrames: return "UnsubscribeFrames";
		case MessageType::Status: return "Status";
		case MessageType::Frame: return "Frame";
		case MessageType::Input: return "Input";
		case MessageType::GetWorldSnapshot: return "GetWorldSnapshot";
		case MessageType::WorldSnapshot: return "WorldSnapshot";
		case MessageType::SetGraphics: return "SetGraphics";
		case MessageType::Command: return "Command";
		case MessageType::CommandResult: return "CommandResult";
		case MessageType::Error: return "Error";
		default: return "Unknown";
		}
	}

} // namespace SFE::EditorProtocol
