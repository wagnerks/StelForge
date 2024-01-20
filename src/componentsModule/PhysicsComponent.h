#pragma once


#include "Jolt/Jolt.h"
#include "Jolt/Physics/Body/BodyID.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "mathModule/Forward.h"


namespace Engine::ComponentsModule {
	class PhysicsComponent {
	public:
		JPH::BodyID mBodyID;

		Engine::Math::Vec3 lastPos = {};
		Engine::Math::Vec3 lastRotate = {};
		bool isSleeping = false;

		PhysicsComponent(const JPH::BodyID& id) : mBodyID(std::move(id)) {}



		~PhysicsComponent();
	};
}

using Engine::ComponentsModule::PhysicsComponent;