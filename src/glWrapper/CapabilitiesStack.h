#pragma once

#include "Capability.h"
#include "StateStack.h"

namespace SFE::GLW {
	template<Capability CapType>
	struct CapabilitiesStack : StateStack<bool, CapabilitiesStack<CapType>> {
		constexpr void apply(bool* state) override {
			if (!state) {
				setCapability(CapType, false);
			}
			else {
				setCapability(CapType, *state);
			}
		}
	};

	template <Capability... CapTypes>
	struct CapabilitiesLock {
		CapabilitiesLock() = default;

		~CapabilitiesLock() {
			pop();
		}

		template<typename... Bools>
		CapabilitiesLock(Bools... states) {
			apply(states...);
		}

		CapabilitiesLock(bool state) {
			(CapabilitiesStack<CapTypes>::push(state), ...);
			
		}

		template<typename... Bools>
		void apply(Bools... states) const {
			(CapabilitiesStack<CapTypes>::push(states), ...);
		}

		void pop() {
			(CapabilitiesStack<CapTypes>::pop(), ...);
		}
	};
}

