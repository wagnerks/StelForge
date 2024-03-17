#include "Buffer.h"

namespace SFE::GLW {
	BindLock::~BindLock() {
		Buffer::bindDefaultBuffer(mType);
	}
}

