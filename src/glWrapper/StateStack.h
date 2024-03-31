#pragma once

#include <stack>

namespace SFE::GLW {
	template<class StateData, class STACK>
	struct StateStack {//may be this should exists per opengl context, but now i have only one context todo
		constexpr static void push(const StateData& data) { instance()->pushImpl(data); }
		constexpr static void pop() {	instance()->popImpl(); }
		static StateData* top() { return instance()->mStack.empty() ? nullptr : &instance()->mStack.top(); }

		virtual ~StateStack() = default;
	private:
		constexpr static StateStack* instance() {
			if (!mInstance) {
				mInstance = new STACK();
			}

			return mInstance;
		}

		constexpr void pushImpl(StateData data) {
			if (mStack.empty()) {
				apply(&data);
			}
			else {
				if (data != mStack.top()) {
					apply(&data);
				}
			}

			mStack.push(std::move(data));
		}

		constexpr void popImpl() {
			if (mStack.empty()) {
				apply(nullptr);
				return;
			}
			auto popped = mStack.top();
			mStack.pop();
			if (mStack.empty()) {
				apply(nullptr);
				return;
			}

			if (mStack.top() != popped) {
				apply(&mStack.top());
			}
		}

		
	private:
		constexpr virtual void apply(StateData* data) = 0;
		thread_local static inline StateStack* mInstance = nullptr;
		std::stack<StateData> mStack;
	};
}
