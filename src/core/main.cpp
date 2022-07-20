#include "main.h"

#include "Engine.h"

int main() {
	const auto engine = GameEngine::Engine::getInstance();
	engine->init();


	while(engine->isAlive()) {
		engine->update();
	}

	GameEngine::Engine::terminate();

	return 0;
}
