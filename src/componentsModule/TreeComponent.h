#pragma once
#include "ecss/Registry.h"
#include "ecss/Types.h"
#include "componentsModule/ComponentBase.h"
#include "containersModule/TreeNode.h"

namespace SFE::ComponentsModule {
	class TreeComponent : public ecss::ComponentInterface {
	public:
		TreeComponent(ecss::SectorId id) : ComponentInterface(id) {};
		~TreeComponent() override;
		ecss::SectorId getParent() const;
		void addChildEntityForce(ecss::SectorId id);

		void removeChildEntityForce(ecss::SectorId id);
		void addChildEntity(ecss::SectorId id);
		void removeChildEntity(ecss::SectorId id);
		const std::vector<ecss::SectorId>& getChildren();

		std::vector<ecss::SectorId> getAllNodes();
		

	private:
		void getAllNodesHelper(std::vector<ecss::SectorId>& res);
		void setParent(ecss::SectorId id);

		ecss::SectorId mParentEntity = ecss::INVALID_ID;
		std::vector<ecss::SectorId> mChildrenEntities;
	};
}

using SFE::ComponentsModule::TreeComponent;