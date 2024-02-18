#pragma once


#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "mathModule/Forward.h"


namespace SFE::ComponentsModule {
	class PhysicsComponent {
	public:
		JPH::BodyID mBodyID;

		SFE::Math::Vec3 lastPos = {};
		SFE::Math::Vec3 lastRotate = {};
		bool isSleeping = false;

		PhysicsComponent(const JPH::BodyID& id) : mBodyID(id) {}



		~PhysicsComponent();
	};
}

using SFE::ComponentsModule::PhysicsComponent;