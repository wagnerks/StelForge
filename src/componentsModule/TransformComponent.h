#pragma once
#include <fwd.hpp>
#include <vec3.hpp>
#include "matrix.hpp"

#include "Component.h"


namespace GameEngine {
	namespace CoreModule {
		class Node;
	}
}

namespace GameEngine::ComponentsModule {

	class TransformComponent : public Component {
	public:
		TransformComponent(ComponentHolder* holder) : Component(holder){}
		void updateComponent() override;

		const glm::vec3& getPos();
		void setX(float x);
		void setY(float y);
		void setZ(float z);

		void setPos(glm::vec3 pos);

		const glm::vec3& getRotate();
		void setRotateX(float x);
		void setRotateY(float y);
		void setRotateZ(float z);
		void setRotate(glm::vec3 rotate);

		const glm::vec3& getScale();
		void setScaleX(float x);
		void setScaleY(float y);
		void setScaleZ(float z);
		void setScale(glm::vec3 scale);

		const glm::mat4& getTransform();
		glm::mat4 getLocalTransform();

		void reloadTransform();
		
		void setOwnerNode(GameEngine::CoreModule::Node* node);
		void markDirty();
	private:
		GameEngine::CoreModule::Node* ownerNode = nullptr;

		bool dirty = true;
		glm::mat4 transform = glm::mat4(1.0f);
		glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 rotate = glm::vec3(0.0f, 0.0f, 0.0f);
	};

}

using GameEngine::ComponentsModule::TransformComponent;