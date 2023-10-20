#pragma once
#include "ecss/Registry.h"
#include "ecss/Types.h"
#include "componentsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class TreeComponent : public ecss::ComponentInterface {
	public:
		TreeComponent(ecss::SectorId id) : ComponentInterface(id) {};
		~TreeComponent() override;
		ecss::SectorId getParent() const;

		void addChildEntity(ecss::SectorId id);
		void removeChildEntity(ecss::SectorId id);
		const std::vector<ecss::SectorId>& getChildren();

		std::vector<ecss::SectorId> getAllNodes();
		void getAllNodesHelper(std::vector<ecss::SectorId>& res);

	private:
		void setParent(ecss::SectorId id);

		ecss::SectorId mParentEntity = ecss::INVALID_ID;
		std::vector<ecss::SectorId> mChildrenEntities;
	};
}

using Engine::ComponentsModule::TreeComponent;