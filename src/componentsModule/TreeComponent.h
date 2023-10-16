#pragma once
#include "..\ecss\Registry.h"
#include "ecss/Types.h"
#include "componentsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class TreeComponent : public ecss::ComponentInterface {
	public:
		TreeComponent(ecss::EntityId id) : ComponentInterface(id) {};
		~TreeComponent() override;
		ecss::EntityId getParent() const;

		void addChildEntity(ecss::EntityId id);
		void removeChildEntity(ecss::EntityId id);
		const std::vector<ecss::EntityId>& getChildren();

		std::vector<ecss::EntityId> getAllNodes();
		void getAllNodesHelper(std::vector<ecss::EntityId>& res);

	private:
		void setParent(ecss::EntityId id);

		ecss::EntityId mParentEntity = ecss::INVALID_ID;
		std::vector<ecss::EntityId> mChildrenEntities;
	};
}

using Engine::ComponentsModule::TreeComponent;