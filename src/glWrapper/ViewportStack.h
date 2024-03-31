#pragma once

#include "StateStack.h"
#include "glad/glad.h"

namespace SFE::GLW {
	struct ViewportState {
		struct Vec2 {
			constexpr friend bool operator==(const Vec2& lhs, const Vec2& rhs) {
				return lhs.x == rhs.x
					&& lhs.y == rhs.y;
			}

			constexpr friend bool operator!=(const Vec2& lhs, const Vec2& rhs) {
				return !(lhs == rhs);
			}

			int x = 0;
			int y = 0;
		};

		constexpr friend bool operator==(const ViewportState& lhs, const ViewportState& rhs) {
			return lhs.size == rhs.size
				&& lhs.pos == rhs.pos;
		}

		constexpr friend bool operator!=(const ViewportState& lhs, const ViewportState& rhs) {
			return !(lhs == rhs);
		}

		Vec2 size;
		Vec2 pos;
	};

	struct ViewportStack : StateStack<ViewportState, ViewportStack> {
		constexpr void apply(ViewportState* state) override {
			if (!state) {
				glViewport(0, 0, size.x, size.y);
				return;
			}
			
			glViewport(state->pos.x, state->pos.y, state->size.x, state->size.y);
		}
		thread_local static inline ViewportState::Vec2 size;
	};
}
