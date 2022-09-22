#include "ModelEntity.h"

#include "componentsModule/LodComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/RenderComponent.h"
#include "componentsModule/TransformComponent.h"
#include "ecsModule/EntityManager.h"

using namespace GameEngine::EntitiesModule;

Model::Model(size_t entId) : Entity<Model>(entId) {
	setNodeId("model");
	auto tc = addComponent<TransformComponent>();	
	auto lc = addComponent<LodComponent>();
	auto rc = addComponent<RenderComponent>();
}

void Model::init(ModelModule::MeshNode& model) {
	
	std::vector<std::vector<ModelModule::MeshHandle>> meshes;


	int i = 0;
	for (auto& mesh : model.mMeshes[i]) {
		std::vector<ModelModule::MeshHandle> lods;
		int j = 0;
		while (model.mMeshes.contains(j)) {
			lods.emplace_back(mesh);
			j++;
		}

		i++;
		meshes.emplace_back(lods);
	}

	for (auto& lods : meshes) {
		auto childModel = ecsModule::ECSHandler::entityManagerInstance()->createEntity<Model>();
		addElement(childModel);
		childModel->addComponent<MeshComponent>(lods);
	}
	
	

	//auto mec = addComponent<MeshComponent>(meshesLods);

	for (auto modelElement : model.getElements()) {
		auto childModel = ecsModule::ECSHandler::entityManagerInstance()->createEntity<Model>();
		addElement(childModel);
		childModel->init(*modelElement);
	}
}
