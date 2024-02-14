#pragma once

#include <shared_mutex>
#include <vector>
#include <json/value.h>

#include "ComponentBase.h"
#include "assetsModule/modelModule/Animation.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/modelModule/Model.h"
#include "ecss/Types.h"
#include "propertiesModule/Serializable.h"


namespace SFE::ComponentsModule {
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

	struct AABBComponent {
		AABBComponent() = default;
		AABBComponent(const AABBComponent& other)
			: aabbs(other.aabbs) {}

		AABBComponent(AABBComponent&& other) noexcept
			: aabbs(std::move(other.aabbs)) {}

		AABBComponent& operator=(const AABBComponent& other) {
			if (this == &other)
				return *this;
			aabbs = other.aabbs;
			return *this;
		}

		AABBComponent& operator=(AABBComponent&& other) noexcept {
			if (this == &other)
				return *this;
			aabbs = std::move(other.aabbs);
			return *this;
		}

		std::vector<FrustumModule::AABB> aabbs;
		std::shared_mutex mtx;
	};

	class ModelComponent : public ecss::ComponentInterface, public PropertiesModule::Serializable {
	public:
		ModelComponent(ecss::SectorId id) : ComponentInterface(id) {};
		void init(AssetsModule::Model* model) {
			mPath = model->getModelPath();
			addMeshData(model->getAllLODs());
		}

		const AssetsModule::ModelObj& getModel();
		AssetsModule::ModelObj& getModel(size_t LOD) const;
		const AssetsModule::ModelObj& getModelLowestDetails() const;

		void setModel(std::vector<AssetsModule::ModelObj>* data);
		LODData mLOD;

		void serialize(Json::Value& data) override;
		void deserialize(const Json::Value& data) override;
		std::string mPath = "";
	private:
		void addMeshData(std::vector<AssetsModule::ModelObj>* meshData);

		std::vector<AssetsModule::ModelObj>* mModel = nullptr;
	};

	struct AnimationComponent {
		AnimationComponent() {
			transforms.resize(10, Math::Mat4{1.f});
		}

		AssetsModule::Animator animator;
		std::vector<Math::Mat4> transforms;
	};
}

using SFE::ComponentsModule::ModelComponent;