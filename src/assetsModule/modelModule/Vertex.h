#pragma once
#include "ecss/Types.h"
#include "mathModule/Forward.h"

namespace SFE {
#define MAX_BONE_INFLUENCE 4
	struct Vertex3D {
		SFE::Math::Vec3 position;
		SFE::Math::Vec2 texCoords;
		SFE::Math::Vec3 normal;

		SFE::Math::Vec3 tangent;
		SFE::Math::Vec3 biTangent;

		int boneIDs[MAX_BONE_INFLUENCE]{ -1,-1,-1,-1 };
		float weights[MAX_BONE_INFLUENCE]{ 0.f,0.f,0.f,0.f };
		SFE::Math::Vec4 color;
		ecss::EntityId entityId;
	};
	
	struct Vertex2D {
		SFE::Math::Vec2 position;
		SFE::Math::Vec2 texCoords;
		SFE::Math::Vec4 color;
	};
}
