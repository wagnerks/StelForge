#include "PhysicsSystem.h"

#include "componentsModule/TransformComponent.h"

void SFE::SystemsModule::Physics::update(float dt) {
	auto& bodyInterface = physics_system->GetBodyInterface();
	for (const auto& [entity, physicsComp, transform] : ECSHandler::registry().getComponentsArray<PhysicsComponent, TransformComponent>()) {
		if (!&transform) {
			continue;
		}
		if (transform.getPos() != physicsComp.lastPos || transform.getRotate() != physicsComp.lastRotate) {
			const auto& pos = transform.getPos(true);
			auto quat = transform.getQuaternion();
			bodyInterface.SetPositionAndRotation(physicsComp.mBodyID, RVec3Arg{ (float)pos.x, (float)pos.y, (float)pos.z }, Quat(quat.x, quat.y, quat.z, quat.w), EActivation::Activate);
		}

		auto isActive = bodyInterface.IsActive(physicsComp.mBodyID);
		if (isActive == physicsComp.isSleeping) {
			continue;
		}
				
		RVec3 position = bodyInterface.GetCenterOfMassPosition(physicsComp.mBodyID);
				
		physicsComp.lastPos = { position.GetX(), position.GetY(), position.GetZ() };
		transform.setPos({ position.GetX(), position.GetY(), position.GetZ() });
				
		auto rotation = bodyInterface.GetRotation(physicsComp.mBodyID).GetEulerAngles();
		physicsComp.lastRotate = { rotation.GetX(), rotation.GetY(), rotation.GetZ() };
		transform.setRotate({ RadiansToDegrees(rotation.GetX()), RadiansToDegrees(rotation.GetY()), RadiansToDegrees(rotation.GetZ()) });
	}

	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	const int cCollisionSteps = 1;//std::ceil(dt * 60.f);

	// Step the world
	physics_system->Update(dt*2.f, cCollisionSteps, temp_allocator, job_system);
}
