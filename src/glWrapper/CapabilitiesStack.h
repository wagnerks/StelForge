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
}

