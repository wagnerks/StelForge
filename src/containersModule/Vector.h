#pragma once
#include <algorithm>
#include <vector>

namespace SFE {
	template<class _Ty, class _Alloc = std::allocator<_Ty>>
	class Vector : public std::vector<_Ty, _Alloc> {
	public:

		template<class _Pr>
		void sort(_Pr predicate) {
			std::sort(this->begin(), this->end(), predicate);
		}

		void sort() {
			std::sort(this->begin(), this->end());
		}

		template<class _Pr>
		_Ty* find(_Pr predicate) const {
			auto it = std::find_if(this->begin(), this->end(), predicate);
			if (it != this->end()) {
				return const_cast<_Ty*>(&*it);
			}

			return nullptr;
		}

		template<class _Pr>
		_Ty* findReverse(_Pr predicate) const {
			auto it = std::find_if(this->rbegin(), this->rend(), predicate);
			if (it != this->rend()) {
				return const_cast<_Ty*>(&*it);
			}

			return nullptr;
		}

		bool contains(const _Ty& val) const {
			return std::find(this->begin(), this->end(), val) != this->end();
		}

		bool containsSorted(const _Ty& val) const {
			return std::binary_search(this->begin(), this->end(), val);
		}
	};
}

