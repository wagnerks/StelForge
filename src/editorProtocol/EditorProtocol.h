#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SFE::EditorProtocol {

	inline constexpr uint32_t kMagic = 0x53464552u; // 'SFER'
	inline constexpr uint16_t kDefaultPort = 7741;

	enum class MessageType : uint32_t {
		Ping = 1,
		Pong = 2,
		SubscribeFrames = 3,
		UnsubscribeFrames = 4,
		Status = 5,
		Frame = 6,
		Input = 7,
		GetWorldSnapshot = 8,
		WorldSnapshot = 9,
		SetGraphics = 10,
		Error = 255,
	};

	struct PacketHeader {
		uint32_t magic = kMagic;
		uint32_t type = 0;
		uint32_t jsonSize = 0;
		uint32_t binarySize = 0;
	};

	struct FramePayload {
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t format = 0; // 0 = RGBA8
	};

	bool writePacket(std::vector<uint8_t>& out, MessageType type, std::string_view json, const void* binary, uint32_t binarySize);
	bool readPacket(const uint8_t* data, size_t size, PacketHeader& header, std::string& json, std::vector<uint8_t>& binary);
	const char* messageTypeName(MessageType type);

} // namespace SFE::EditorProtocol
