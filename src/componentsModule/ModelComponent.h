﻿#pragma once

#include <vector>
#include <json/value.h>

#include "ecsModule/ComponentBase.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/modelModule/Model.h"
#include "propertiesModule/Serializable.h"


namespace Engine::ComponentsModule {
	struct LODData {
		size_t mLodLevel = 0;
		float mCurrentLodValue = 0.f;

		void setLodLevel(size_t lodLevel);
		size_t getLodLevel() const;

		const std::vector<float>& getLodLevelValues();
		void addLodLevelValue(float lodLevel);

		float getCurrentLodValue() const;
		void setCurrentLodValue(float currentLodValue);

		std::vector<float> mLodLevelValues;
	};

	class ModelComponent : public ecsModule::Component<ModelComponent>, public PropertiesModule::Serializable {
	public:
		ModelComponent();
		void init(AssetsModule::Model* model) {
			mPath = model->getModelPath();
			addMeshData(model->getAllLODs());
		}

		const AssetsModule::ModelObj& getModel();
		AssetsModule::ModelObj& getModel(size_t LOD);
		const AssetsModule::ModelObj& getModelLowestDetails();

		void setModel(std::vector<AssetsModule::ModelObj> data);
		LODData mLOD;

		void serialize(Json::Value& data) override;
		void deserialize(const Json::Value& data) override;
		std::string mPath = "";
	private:
		void addMeshData(std::vector<AssetsModule::ModelObj> meshData);

		size_t modelId = ecsModule::INVALID_ID;
		std::vector<AssetsModule::ModelObj> mModel;

	};
}

using Engine::ComponentsModule::ModelComponent;