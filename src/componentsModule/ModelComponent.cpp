#include "ModelComponent.h"

#include <algorithm>

#include "core/ModelLoader.h"
#include "ecsModule/EntityManager.h"
#include "modelModule/Model.h"


using namespace Engine::ComponentsModule;

void LODData::setLodLevel(size_t lodLevel) {
	mLodLevel = lodLevel;
}

size_t LODData::getLodLevel() const {
	return mLodLevel;
}

const std::vector<float>& LODData::getLodLevelValues() {
	return mLodLevelValues;
}

void LODData::addLodLevelValue(float lodLevel) {
	mLodLevelValues.emplace_back(lodLevel);
	std::ranges::sort(mLodLevelValues, [](auto& a, auto& b) { return a > b; });
}

float LODData::getCurrentLodValue() const {
	return mCurrentLodValue;
}

void LODData::setCurrentLodValue(float currentLodValue) {
	mCurrentLodValue = currentLodValue;
}


ModelComponent::ModelComponent() {}

void ModelComponent::addMeshData(std::vector<AssetsModule::ModelObj> meshData) {
	mModel = std::move(meshData);
}

const AssetsModule::ModelObj& ModelComponent::getModel() {
	return getModel(mLOD.getLodLevel());
}

const AssetsModule::ModelObj& ModelComponent::getModel(size_t LOD) {
	if (mModel.empty()) {
		static AssetsModule::ModelObj empty;
		return empty;
	}

	if (mModel.size() < LOD) {
		return mModel.back();
	}

	return mModel[LOD];
}

const AssetsModule::ModelObj& ModelComponent::getModelLowestDetails() {
	if (mModel.empty()) {
		static AssetsModule::ModelObj empty;
		return empty;
	}

	return mModel.back();
}

void ModelComponent::setModel(std::vector<AssetsModule::ModelObj> data) {
	mModel = std::move(data);
}

bool ModelComponent::serialize(Json::Value& data) {
	if (modelId != ecsModule::INVALID_ID) {
		/*if (auto modelEntity = CoreModule::ModelLoader::instance()->getModelsHolder()->getAsset<ModelModule::Model>()) {
			data["ModelPath"] = modelEntity->getModelPath().data();
		}*/
	}

	return true;
}

bool ModelComponent::deserialize(const Json::Value& data) {
	AssetsModule::Model* model = nullptr;
	if (data.isMember("ModelPath")) {
		model = AssetsModule::ModelLoader::instance()->load(data["ModelPath"].asString());
		//modelId = model->getEntityID();
	}

	if (model) {
		addMeshData(model->getAllLODs());
	}

	return true;
}