#pragma once

#include <shared_mutex>

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

#include "componentsModule/ComponentBase.h"
#include "propertiesModule/Serializable.h"

namespace SFE::ComponentsModule {
	class TransformComponent : public ecss::ComponentInterface, public PropertiesModule::Serializable {
	public:
		TransformComponent(const TransformComponent& other)
			: ecss::ComponentInterface(other),
			  PropertiesModule::Serializable(other),
			  mDirty(other.mDirty),
			  mRotateQuaternion(other.mRotateQuaternion),
			  mTransform(other.mTransform),
			  mPos(other.mPos),
			  mScale(other.mScale),
			  mRotate(other.mRotate) {}

		TransformComponent& operator=(const TransformComponent& other) {
			if (this == &other)
				return *this;
			ecss::ComponentInterface::operator =(other);
			PropertiesModule::Serializable::operator =(other);
			mDirty = other.mDirty;
			mRotateQuaternion = other.mRotateQuaternion;
			mTransform = other.mTransform;
			mPos = other.mPos;
			mScale = other.mScale;
			mRotate = other.mRotate;
			return *this;
		}

		TransformComponent(TransformComponent&& other) noexcept
			: ecss::ComponentInterface(std::move(other)),
			  PropertiesModule::Serializable(std::move(other)),
			  mDirty(other.mDirty),
			  mRotateQuaternion(std::move(other.mRotateQuaternion)),
		      mTransform(std::move(other.mTransform)),
			  mPos(std::move(other.mPos)),
			  mScale(std::move(other.mScale)),
			  mRotate(std::move(other.mRotate)){}

		TransformComponent& operator=(TransformComponent&& other) noexcept {
			if (this == &other)
				return *this;
			ecss::ComponentInterface::operator =(std::move(other));
			PropertiesModule::Serializable::operator =(std::move(other));
			mDirty = other.mDirty;
			mTransform = std::move(other.mTransform);
			mRotateQuaternion = std::move(other.mRotateQuaternion);
			mPos = std::move(other.mPos);
			mScale = std::move(other.mScale);
			mRotate = std::move(other.mRotate);
			return *this;
		}

		TransformComponent(ecss::SectorId id) : ComponentInterface(id) { markDirty(); };
		~TransformComponent() override;

		const Math::Vec3& getPos(bool global = false) const;
		void setX(float x);
		void setY(float y);
		void setZ(float z);

		void setPos(const Math::Vec3& pos);

		const Math::Vec3& getRotate() const;
		void setPitch(float x);
		void setYaw(float y);
		void setRoll(float z);
		void setRotate(const Math::Vec3& rotate);

		const Math::Vec3& getScale() const;
		Math::Vec3 getGlobalScale() const;

		void setScaleX(float x);
		void setScaleY(float y);
		void setScaleZ(float z);
		void setScale(const Math::Vec3& scale);

		const Math::Mat4& getTransform() const;
		void setTransform(const Math::Mat4& transform);

		Math::Mat4 getRotationMatrix() const;
		const Math::Quaternion<float>& getQuaternion() const;
		Math::Mat4 calculateLocalTransform() const;
		Math::Mat4 getViewMatrix() const;

		Math::Vec3 getRight();
		Math::Vec3 getUp();
		Math::Vec3 getBackward();
		Math::Vec3 getForward();

		void reloadTransform();

		void markDirty();
		bool isDirty() const;

		void deserialize(const Json::Value& data) override;
		void serialize(Json::Value& data) override;

	private:
		bool mDirty = false;

		Math::Quaternion<float> mRotateQuaternion;
		Math::Mat4 mTransform = Math::Mat4{ 1.f };

		Math::Vec3 mPos = {0.f};
		Math::Vec3 mScale = { 1.f };
		Math::Vec3 mRotate = { 0.f }; 
		
		mutable std::shared_mutex mtx;
	};

	struct TransformMatComp {
		Math::Mat4 mTransform = Math::Mat4{ 1.f };
	};

}

using SFE::ComponentsModule::TransformComponent;
//using SFE::ComponentsModule::DirtyTransform;