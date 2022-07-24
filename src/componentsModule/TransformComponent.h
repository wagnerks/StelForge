#pragma once
#include <fwd.hpp>
#include <vec3.hpp>
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
		void updateComponent() override;
		void setWithView(bool isWithView);
		bool isWithView();

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

		glm::mat4& getTransform();
		glm::mat4 getRotationMatrix();
		glm::mat4 getLocalTransform();
		const glm::mat4& getViewMatrix() const;
		glm::vec3 getFront();
		void reloadTransform();
		
		void markDirty();
	private:
		bool withView = false;
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