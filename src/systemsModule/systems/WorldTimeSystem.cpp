#include "WorldTimeSystem.h"

void Engine::SystemsModule::WorldTimeSystem::update(float dt) {
	mWorldTime += dt;
}
