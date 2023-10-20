#pragma once
#include <fwd.hpp>
#include <vector>
#include <detail/type_quat.hpp>

#include "matrix.hpp"

#include "componentsModule/ComponentBase.h"
#include "propertiesModule/Serializable.h"


namespace Engine::ComponentsModule {

	class DirtyTransform {};

	class TransformComponent : public ecss::ComponentInterface, public PropertiesModule::Serializable {
	public:
		TransformComponent(ecss::SectorId id) : ComponentInterface(id) {};
		~TransformComponent() override;

		const glm::vec3& getPos(bool global = false) const;
		void setX(float x);
		void setY(float y);
		void setZ(float z);

		void setPos(const glm::vec3& pos);

		const glm::vec3& getRotate() const;
		void setRotateX(float x);
		void setRotateY(float y);
		void setRotateZ(float z);
		void setRotate(const glm::vec3& rotate);
		void setRotation(const glm::quat& rotate);

		const glm::vec3& getScale(bool global = false) const;
		void setScaleX(float x);
		void setScaleY(float y);
		void setScaleZ(float z);
		void setScale(const glm::vec3& scale);

		const glm::mat4& getTransform() const;
		void setTransform(const glm::mat4& transform);

		glm::mat4 getRotationMatrix() const;
		glm::quat getRotationMatrixQuaternion() const;
		glm::mat4 getLocalTransform() const;
		const glm::mat4& getViewMatrix() const;

		glm::vec3 getRight();
		glm::vec3 getUp();
		glm::vec3 getBackward();
		glm::vec3 getForward();

		void reloadTransform();

		void markDirty();
		bool isDirty() const;

		void deserialize(const Json::Value& data) override;
		void serialize(Json::Value& data) override;
	private:
		glm::vec3 calculateGlobalScale();

		bool dirty = true;
		glm::mat4 transform = glm::mat4(1.0f);
		glm::mat4 view = glm::mat4(1.0f);

		glm::quat rotateQuat = {};
		glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 rotate = glm::vec3(0.0f, 0.0f, 0.0f);

		glm::vec3 globalScale = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 globalPos = glm::vec3(1.0f, 1.0f, 1.0f);
	};

}

using Engine::ComponentsModule::TransformComponent;
using Engine::ComponentsModule::DirtyTransform;