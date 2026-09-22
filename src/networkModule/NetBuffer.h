#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace SFE::Network {
	// A message's bytes, written field by field as they lie in memory: every machine that plays is little-endian x86
	// and runs the same build, so nothing is swapped or packed.
	class NetWriter {
	public:
		template <typename T>
		NetWriter& write(const T& value) {
			static_assert(std::is_trivially_copyable_v<T>);
			const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
			mBytes.insert(mBytes.end(), bytes, bytes + sizeof(T));
			return *this;
		}

		NetWriter& writeString(std::string_view text) {
			write(static_cast<uint16_t>(text.size()));
			mBytes.insert(mBytes.end(), text.begin(), text.end());
			return *this;
		}

		const std::vector<uint8_t>& bytes() const { return mBytes; }
		size_t size() const { return mBytes.size(); }
		void clear() { mBytes.clear(); }

	private:
		std::vector<uint8_t> mBytes;
	};

	// Reads what a NetWriter wrote, in the same order. Reading past the end gives zeros and makes ok() false, so a
	// short or malformed message is dropped by its handler instead of read out of bounds.
	class NetReader {
	public:
		NetReader(const uint8_t* data, size_t size) : mData(data), mSize(size) {}

		template <typename T>
		T read() {
			static_assert(std::is_trivially_copyable_v<T>);
			T value{};
			if (mPos + sizeof(T) > mSize) {
				mOk = false;
				mPos = mSize;
				return value;
			}
			std::memcpy(&value, mData + mPos, sizeof(T));
			mPos += sizeof(T);
			return value;
		}

		std::string readString() {
			const auto length = read<uint16_t>();
			if (mPos + length > mSize) {
				mOk = false;
				mPos = mSize;
				return {};
			}
			std::string text(reinterpret_cast<const char*>(mData + mPos), length);
			mPos += length;
			return text;
		}

		bool ok() const { return mOk; }
		bool atEnd() const { return mPos >= mSize; }

	private:
		const uint8_t* mData = nullptr;
		size_t mSize = 0;
		size_t mPos = 0;
		bool mOk = true;
	};
}
