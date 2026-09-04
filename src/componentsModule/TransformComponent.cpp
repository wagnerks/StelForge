#include "TransformComponent.h"

#include "TreeComponent.h"
#include "core/ECSHandler.h"
#include "multithreading/ThreadPool.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/systems/TransformSystem.h"

namespace SFE::ComponentsModule {
	const Math::Vec3& TransformComponent::getPos(bool global) const {
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
		if (mPos != pos) {	markDirty(); }

		mPos = pos;
	}

	//x - pitch, y - yaw, z - roll
	const Math::Vec3& TransformComponent::getRotate() const {
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
		if (this->mRotate != rotate) { markDirty(); }

		this->mRotate = rotate;
	}

	const Math::Vec3& TransformComponent::getScale() const {
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
		if (mScale != scale) { markDirty();	}

		mScale = scale;
	}

	const Math::Mat4& TransformComponent::getTransform() const {
		return mTransform;
	}

	void TransformComponent::setTransform(const SFE::Math::Mat4& transform) {
		mTransform = transform;
	}

	Math::Mat4 TransformComponent::getRotationMatrix() const {
		return mRotateQuaternion.toMat4();
	}

	const Math::Quaternion<float>& TransformComponent::getQuaternion() const {
		return mRotateQuaternion;
	}

	void TransformComponent::reloadTransform() {
		if (!mDirty) {
			return;
		}
		mDirty = false;

		mRotateQuaternion.eulerToQuaternion(mRotate);

		auto newTransform = calculateLocalTransform();
		
		if (const auto tree = ECSHandler::registry().pinComponent<const TreeComponent>(getEntityId())) {
			if (const auto parentTransform = ECSHandler::registry().pinComponent<const TransformComponent>(tree->getParent())) {
				newTransform = parentTransform->getTransform() * newTransform;
			}

			mTransform = std::move(newTransform);

			for (const auto childTransform : tree->getChildren()) {
				if (auto transformPtr = ECSHandler::registry().pinComponent<TransformComponent>(childTransform)) {
					transformPtr->markDirty();
				}
			}
		}
		else {
			mTransform = std::move(newTransform);
		}
	}

	SFE::Math::Mat4 TransformComponent::calculateLocalTransform() const {
		return Math::translate(Math::Mat4{1.f}, mPos) * mRotateQuaternion.toMat4() * Math::scale(Math::Mat4{ 1.f }, mScale);
	}

	SFE::Math::Mat4 TransformComponent::getViewMatrix() const {
		return inverse(mTransform);;
	}

	SFE::Math::Vec3 TransformComponent::getRight() const {
		return mTransform[0];
	}

	SFE::Math::Vec3 TransformComponent::getUp() const {
		return mTransform[1];
	}

	SFE::Math::Vec3 TransformComponent::getBackward() const {
		return mTransform[2];
	}

	SFE::Math::Vec3 TransformComponent::getForward() const {
		return -mTransform[2];
	}

	void TransformComponent::markDirty() {
		if (mDirty) {
			return;
		}
		mDirty = true;

		SystemsModule::TasksManager::instance()->notify({ getEntityId(), SystemsModule::TRANSFORM_UPDATE });
	}

	bool TransformComponent::isDirty() const {
		return mDirty;
	}
}