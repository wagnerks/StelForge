#pragma once

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

#include "componentsModule/ComponentBase.h"

namespace SFE::ComponentsModule {
	/**
	 * @brief Position, rotation and scale of one entity.
	 *
	 * Trivially copyable, and worth keeping that way: this is the component every frame walks,
	 * so it is the one whose array benefits most from ecss relocating sectors with a memmove
	 * instead of a move constructor per element.
	 *
	 * Values are not synchronised. The component used to hold a shared_mutex, but every getter
	 * handed out a reference that outlived the lock, so it never provided the guarantee it
	 * looked like it did. Order reads against writes the way ecss intends -- claim the type for
	 * the duration of a system with `registry.access<Read<TransformComponent>>()` -- rather
	 * than per component.
	 */
	class TransformComponent : public ecss::ComponentInterface {
	public:
		TransformComponent(ecss::SectorId id) : ComponentInterface(id) { markDirty(); };

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

		Math::Vec3 getRight() const;
		Math::Vec3 getUp() const;
		Math::Vec3 getBackward() const;
		Math::Vec3 getForward() const;

		void reloadTransform();

		void markDirty();
		bool isDirty() const;

	private:
		bool mDirty = false;

		Math::Quaternion<float> mRotateQuaternion;
		Math::Mat4 mTransform = Math::Mat4{ 1.f };

		Math::Vec3 mPos = {0.f};
		Math::Vec3 mScale = { 1.f };
		Math::Vec3 mRotate = { 0.f };
	};

	struct TransformMatComp {
		Math::Mat4 mTransform = Math::Mat4{ 1.f };
	};

}

using SFE::ComponentsModule::TransformComponent;
//using SFE::ComponentsModule::DirtyTransform;