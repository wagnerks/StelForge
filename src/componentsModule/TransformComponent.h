#pragma once
#include <fwd.hpp>
#include <vec3.hpp>
#include <vector>
#include <detail/type_quat.hpp>

#include "matrix.hpp"

#include "Component.h"

namespace GameEngine {
	namespace NodeModule {
		class Node;
	}
}

namespace GameEngine::ComponentsModule {

	class TransformComponent : public Component {
	public:
		TransformComponent(ComponentHolder* holder);
		~TransformComponent() override;
		void updateComponent() override;

		void addChildTransform(TransformComponent* child);
		void removeChildTransform(TransformComponent* child);

		glm::vec3 getPos(bool global = false) const;
		void setX(float x);
		void setY(float y);
		void setZ(float z);

		void setPos(glm::vec3 pos);

		const glm::vec3& getRotate() const;
		void setRotateX(float x);
		void setRotateY(float y);
		void setRotateZ(float z);
		void setRotate(glm::vec3 rotate);

		const glm::vec3& getScale() const;
		void setScaleX(float x);
		void setScaleY(float y);
		void setScaleZ(float z);
		void setScale(glm::vec3 scale);

		const glm::mat4& getTransform() const;
		glm::mat4 getRotationMatrix() const;
		glm::mat4 getLocalTransform() const;
		glm::mat4 getViewMatrix() const;

		glm::vec3 getRight();
		glm::vec3 getUp();
		glm::vec3 getBackward();
		glm::vec3 getForward();

		void reloadTransform();
		
		void markDirty();
	private:
		std::vector<TransformComponent*> childTransforms; //todo dirty, possible crashes
		TransformComponent* parentTransform = nullptr;
		NodeModule::Node* ownerNode = nullptr;

		bool dirty = true;
		glm::mat4 transform = glm::mat4(1.0f);
		glm::mat4 viewMatrix = glm::mat4(0.0f);

		glm::quat rotateQuat = {};
		glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 rotate = glm::vec3(0.0f, 0.0f, 0.0f);
	};

}

using GameEngine::ComponentsModule::TransformComponent;