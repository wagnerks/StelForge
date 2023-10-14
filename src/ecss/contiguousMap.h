#pragma once
#include <algorithm>

namespace ecss {
	template<typename Key, typename Value>
	class ContiguousMap {
		struct KeyValPair {
			Key key;
			Value value;
		};
	public:
		ContiguousMap(const ContiguousMap& other)
			: mSize(other.mSize),
			mCapacity(other.mCapacity) {
			mData = new KeyValPair[mCapacity];
			for (auto i = 0; i < mSize; i++) {
				mData[i] = other.mData[i];
			}
		}
		ContiguousMap(ContiguousMap&& other) noexcept
			: mSize(other.mSize),
			mCapacity(other.mCapacity),
			mData(other.mData) {
			other.mData = nullptr;
		}
		ContiguousMap& operator=(const ContiguousMap& other) {
			if (this == &other)
				return *this;
			mSize = other.mSize;
			mCapacity = other.mCapacity;
			mData = new KeyValPair[mCapacity];
			for (auto i = 0; i < mSize; i++) {
				mData[i] = other.mData[i];
			}
			return *this;
		}
		ContiguousMap& operator=(ContiguousMap&& other) noexcept {
			if (this == &other)
				return *this;
			mSize = other.mSize;
			mCapacity = other.mCapacity;
			mData = other.mData;
			other.mData = nullptr;
			return *this;
		}

		ContiguousMap() = default;
		~ContiguousMap() {
			delete[] mData;
		}

		Value& operator[](Key key) {
			for (auto i = 0u; i < mSize; i++) {
				if (mData[i].key == key) {
					return mData[i].value;
				}
			}

			if (mSize >= mCapacity) {
				if (mCapacity == 0) {
					mCapacity = 1;
					mData = new KeyValPair[mCapacity];
				}
				else {
					mCapacity *= 2;
					auto newData = new KeyValPair[mCapacity];
					for (auto i = 0u; i < mSize; i++) {
						newData[i] = std::move(mData[i]);
					}

					delete[] mData;
					mData = newData;
				}
			}

			mData[mSize].key = key;
			return mData[mSize++].value;
		}

		Value& insert(Key key, Value value) {
			for (auto i = 0u; i < mSize; i++) {
				if (mData[i].key == key) {
					mData[i].value = std::move(value);
					return mData[i].value;
				}
			}

			if (mCapacity <= mSize) {
				if (mCapacity == 0) {
					mCapacity = 1;
					mData = new KeyValPair[mCapacity];
				}
				else {
					mCapacity *= 2;
					auto newData = new KeyValPair[mCapacity];
					for (auto i = 0u; i < mSize; i++) {
						newData[i] = std::move(mData[i]);
					}

					delete[] mData;
					mData = newData;
				}
			}

			mData[mSize].key = key;
			mData[mSize].value = std::move(value);

			return mData[mSize++].value;
		}

		std::pair<Key,Value> find(Key key) {
			for (auto i = 0u; i < mSize; i++) {
				if (mData[i].key == key) {
					return { mData[i].key, mData[i].value };
				}
			}

			return {};
		}

		bool contains(Key key) {
			for (auto i = 0u; i < mSize; i++) {
				if (mData[i].key == key) {
					return true;
				}
			}

			return false;
		}

		class Iterator {
		public:
			KeyValPair* ptr;
			Iterator(KeyValPair* ptr) : ptr(ptr) {}

			std::pair<Key&,Value&> operator*() const {
				return { ptr->key, ptr->value };
			}

			bool operator!=(Iterator& other) const {
				return ptr != other.ptr;
			}

			Iterator& operator++() {
				return ++ptr, *this;
			}
		};

		Iterator begin() const {
			return { mData };
		}

		Iterator end() const {
			return { mData + mSize };
		}

	private:
		size_t mSize = 0;
		size_t mCapacity = 0;

		KeyValPair* mData = nullptr;
	};
}
