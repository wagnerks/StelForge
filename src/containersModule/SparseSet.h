#pragma once
#include <vector>

namespace SFE {
	template<typename T, typename KeyType = size_t>
	struct SparseSet {
		constexpr static inline KeyType INVALID_IDX = std::numeric_limits<KeyType>::max();

		T& operator[](KeyType key) {
			auto idx = getIndex(key);
			if (idx == INVALID_IDX) {
				mDense.emplace_back(T{});
				if (mSparce.size() <= key) {
					mSparce.resize(key + 1, INVALID_IDX);
				}
				idx = mDense.size() - 1;
				mSparce[key] = static_cast<KeyType>(idx);
			}

			return mDense[idx];
		}

		KeyType getKey(size_t index) const {
			for (auto i = 0u; i < mSparce.size(); i++) {
				if (mSparce[i] == index) {
					return i;
				}
			}

			return INVALID_IDX;
		}

		void insert(KeyType key, const T& obj) {
			auto idx = getIndex(key);
			if (idx != INVALID_IDX) {
				return;
			}

			mDense.emplace_back(obj);
			if (mSparce.size() <= key) {
				mSparce.resize(key + 1, INVALID_IDX);
			}
			mSparce[key] = static_cast<KeyType>(mDense.size() - 1);
		}

		typename std::vector<T>::iterator begin() {
			return mDense.begin();
		}
		typename std::vector<T>::iterator end() {
			return mDense.end();
		}

	private:

		inline size_t getIndex(KeyType key) const {
			return static_cast<size_t>(key >= mSparce.size() ? INVALID_IDX : mSparce[key]);
		}

		std::vector<KeyType> mSparce;//indexes
		std::vector<T> mDense;//entities
	};
}
