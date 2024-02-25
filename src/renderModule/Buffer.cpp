#include "Buffer.h"

namespace SFE::Render {
	BindGuard::~BindGuard() {
		Buffer::bindDefaultBuffer(mType);
	}
}

