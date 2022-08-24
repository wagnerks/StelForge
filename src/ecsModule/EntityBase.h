﻿#pragma once
#include "ECSHandler.h"
#include "helper.h"
#include "ComponentsManager.h"

namespace ecsModule {
	class EntityInterface {
		friend class EntityManager;
	public:
		size_t getEntityID() const;
		void setId(size_t id);
		virtual size_t getStaticTypeID() const = 0;

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
	private:
		size_t mId = INVALID_ID;
	protected:
		EntityInterface(size_t entityID) : mId(entityID){};
		virtual ~EntityInterface();
	};

	template<class T>
	class Entity : public EntityInterface {
	public:
		Entity(size_t entID) : EntityInterface(entID){};
		size_t getStaticTypeID() const override { return STATIC_ENTITY_TYPE_ID;}

		inline static const size_t STATIC_ENTITY_TYPE_ID = FamilySize<EntityInterface>::Get<T>();
	};
}
