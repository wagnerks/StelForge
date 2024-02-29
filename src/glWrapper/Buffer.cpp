#include "Buffer.h"

namespace SFE::GLW {
	BindGuard::~BindGuard() {
		Buffer::bindDefaultBuffer(mType);
	}
}

