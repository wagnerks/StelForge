#pragma once
#include <functional>

#include "ECSHandler.h"
#include "helper.h"
#include "ComponentsManager.h"
#include "componentsModule/TransformComponent.h"
#include "nodeModule/TreeNode.h"

namespace ecsModule {
	class EntityInterface : public Engine::NodeModule::TreeNode<EntityInterface> {
		friend class EntityManagerInterface;
		friend class TreeNode;
	public:
		virtual size_t getStaticTypeID() const = 0;

		size_t getEntityID() const;
		void setId(size_t id);

		template<typename E, class ... Args>
		E* getComponent() const {
			if (auto cmp = ECSHandler::componentManagerInstance()->getComponent<E>(getEntityID())) {
				return cmp;
			}
			return nullptr;
		}

		template<typename E, class ... Args>
		E* addComponent(Args&&... args) const {
			return ECSHandler::componentManagerInstance()->addComponent<E>(getEntityID(), std::forward<Args>(args)...);;
		}

		template<typename E>
		void removeComponent() const {
			ECSHandler::componentManagerInstance()->removeComponent<E>(getEntityID());
		}

		void releaseComponents() const {
			ECSHandler::componentManagerInstance()->removeAllComponents(getEntityID());
		}

		void operator delete(void* ptr);

	protected:
		EntityInterface(size_t entityID);
		~EntityInterface() override;

	private:
		size_t mId = INVALID_ID;
	};

	template<class T>
	class Entity : public EntityInterface {
	public:
		Entity(size_t entID) : EntityInterface(entID) {};
		size_t getStaticTypeID() const override { return STATIC_ENTITY_TYPE_ID; }

		inline static const size_t STATIC_ENTITY_TYPE_ID = FamilySize<EntityInterface>::Get<T>();
	};
}

