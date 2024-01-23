#include "PhysicsComponent.h"
#include "systemsModule/SystemManager.h"
#include "core/ECSHandler.h"
#include "systemsModule/systems/PhysicsSystem.h"

PhysicsComponent::~PhysicsComponent() {
	if (ECSHandler::isAlive()) {
		if (auto system = ECSHandler::getSystem<SFE::SystemsModule::Physics>()) {
			JPH::BodyInterface& body_interface = system->physics_system->GetBodyInterface();

			body_interface.RemoveBody(mBodyID);
			body_interface.DestroyBody(mBodyID);
		}
	}
}
