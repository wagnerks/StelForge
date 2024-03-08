#include "TransformComponent.h"

#include "TreeComponent.h"
#include "multithreading/ThreadPool.h"
#include "propertiesModule/PropertiesSystem.h"
#include "propertiesModule/TypeName.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/systems/TransformSystem.h"

namespace SFE::ComponentsModule {
	const Math::Vec3& TransformComponent::getPos(bool global) const {
		std::shared_lock lock(mtx);
		if (global) {
			return mTransform[3].xyz;
		}
		return mPos;
	}

	void TransformComponent::setX(float x) {
		setPos({ x, mPos.y, mPos.z });
	}
	void TransformComponent::setY(float y) {
		setPos({ mPos.x, y, mPos.z });
	}
	void TransformComponent::setZ(float z) {
		setPos({ mPos.x, mPos.y, z });
	}
	void TransformComponent::setPos(const Math::Vec3& pos) {
		std::unique_lock lock(mtx);
		if (mPos != pos) {	markDirty(); }

		mPos = pos;
	}

	//x - pitch, y - yaw, z - roll
	const Math::Vec3& TransformComponent::getRotate() const {
		std::shared_lock lock(mtx);
		return mRotate;
	}

	void TransformComponent::setPitch(float x) {
		setRotate({ x, mRotate.y, mRotate.z });
	}
	void TransformComponent::setYaw(float y) {
		setRotate({ mRotate.x, y, mRotate.z });
	}
	void TransformComponent::setRoll(float z) {
		setRotate({ mRotate.x, mRotate.y, z });
	}
	void TransformComponent::setRotate(const SFE::Math::Vec3& rotate) {
		std::unique_lock lock(mtx);
		if (this->mRotate != rotate) { markDirty(); }

		this->mRotate = rotate;
	}

	const Math::Vec3& TransformComponent::getScale() const {
		std::shared_lock lock(mtx);
		return mScale;
	}

	Math::Vec3 TransformComponent::getGlobalScale() const {
		//return { glm::length(mTransform[0]), glm::length(mTransform[1]), glm::length(mTransform[2]) };
		return { Math::length(mTransform[0]), Math::length(mTransform[1]), Math::length(mTransform[2]) };
	}
	void TransformComponent::setScaleX(float x) {
		setScale({ x, mScale.y, mScale.z });
	}
	void TransformComponent::setScaleY(float y) {
		setScale({ mScale.x, y, mScale.z });
	}
	void TransformComponent::setScaleZ(float z) {
		setScale({ mScale.x, mScale.y, z });
	}
	void TransformComponent::setScale(const SFE::Math::Vec3& scale) {
		std::unique_lock lock(mtx);
		if (mScale != scale) { markDirty();	}

		mScale = scale;
	}

	const Math::Mat4& TransformComponent::getTransform() const {
		//std::shared_lock lock(mtx);
		return mTransform;
	}

	void TransformComponent::setTransform(const SFE::Math::Mat4& transform) {
		std::unique_lock lock(mtx);
		mTransform = transform;
	}

	Math::Mat4 TransformComponent::getRotationMatrix() const {
		std::shared_lock lock(mtx);
		return mRotateQuaternion.toMat4();
	}

	const Math::Quaternion<float>& TransformComponent::getQuaternion() const {
		std::shared_lock lock(mtx);
		return mRotateQuaternion;
	}

	void TransformComponent::reloadTransform() {
		{
			auto lock = std::unique_lock(mtx);
			if (!mDirty) {
				return;
			}
			mDirty = false;
		}

		{
			auto lock = std::unique_lock(mtx);
			mRotateQuaternion.eulerToQuaternion(mRotate);
		}
		
		auto newTransform = calculateLocalTransform();

		if (const auto tree = ECSHandler::registry().getComponent<TreeComponent>(getEntityId())) {
			if (const auto parentTransform = ECSHandler::registry().getComponentForce<TransformComponent>(tree->getParent())) {
				newTransform = parentTransform->getTransform() * newTransform;
			}

			{
				auto lock = std::unique_lock(mtx);
				mTransform = std::move(newTransform);
			}

			for (const auto childTransform : tree->getChildren()) {
				if (auto transformPtr = ECSHandler::registry().getComponentForce<TransformComponent>(childTransform)) {
					transformPtr->mDirty = true;
					transformPtr->reloadTransform();
				}
			}
		}
		else {
			auto lock = std::unique_lock(mtx);
			mTransform = std::move(newTransform);
		}
	}

	SFE::Math::Mat4 TransformComponent::calculateLocalTransform() const {
		return Math::translate(Math::Mat4{1.f}, mPos) * mRotateQuaternion.toMat4() * Math::scale(Math::Mat4{ 1.f }, mScale);
	}

	SFE::Math::Mat4 TransformComponent::getViewMatrix() const {
		std::shared_lock lock(mtx);
		return inverse(mTransform);;
	}

	SFE::Math::Vec3 TransformComponent::getRight() {
		std::shared_lock lock(mtx);
		return mTransform[0];
	}

	SFE::Math::Vec3 TransformComponent::getUp() {
		std::shared_lock lock(mtx);
		return mTransform[1];
	}

	SFE::Math::Vec3 TransformComponent::getBackward() {
		std::shared_lock lock(mtx);
		return mTransform[2];
	}

	SFE::Math::Vec3 TransformComponent::getForward() {
		std::shared_lock lock(mtx);
		return -mTransform[2];
	}

	void TransformComponent::markDirty() {
		if (mDirty) {
			return;
		}
		mDirty = true;

		if (auto trSys = ECSHandler::getSystem<SystemsModule::TransformSystem>()) {
			trSys->addDirtyComp(getEntityId());
		}
	}

	bool TransformComponent::isDirty() const {
		std::shared_lock lock(mtx);
		return mDirty;
	}

	void TransformComponent::serialize(Json::Value& data) {
		std::shared_lock lock(mtx);

		data["Scale"].append(mScale.x);
		data["Scale"].append(mScale.y);
		data["Scale"].append(mScale.z);

		data["Pos"].append(mPos.x);
		data["Pos"].append(mPos.y);
		data["Pos"].append(mPos.z);

		data["Rotate"].append(mRotate.x);
		data["Rotate"].append(mRotate.y);
		data["Rotate"].append(mRotate.z);
	}

	void TransformComponent::deserialize(const Json::Value& data) {
		if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Scale")) {
			setScale(PropertiesModule::JsonUtils::getVec3(*val));
		}

		if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Pos")) {
			setPos(PropertiesModule::JsonUtils::getVec3(*val));
		}

		if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Rotate")) {
			setRotate(PropertiesModule::JsonUtils::getVec3(*val));
		}
	}

	TransformComponent::~TransformComponent() {

	}
}