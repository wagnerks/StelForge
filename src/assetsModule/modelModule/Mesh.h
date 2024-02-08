#pragma once

#include <functional>
#include <string>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "core/BoundingVolume.h"
#include "ecss/Types.h"


namespace AssetsModule {
	struct Vertex {
		SFE::Math::Vec3 mPosition;
		SFE::Math::Vec3 mNormal;
		SFE::Math::Vec2 mTexCoords;
		SFE::Math::Vec3 mTangent;
		SFE::Math::Vec3 mBiTangent;
	};

	struct MaterialTexture {
		Texture* mTexture;
		std::string mType;
	};

	struct Material {
		MaterialTexture mDiffuse;
		MaterialTexture mNormal;
		MaterialTexture mSpecular;
	};

	struct MeshData {
		std::vector<Vertex> mVertices;
		std::vector<unsigned int> mIndices;

		unsigned int mVao = std::numeric_limits<unsigned>::max();
		unsigned int mVbo = std::numeric_limits<unsigned>::max();
		unsigned int mEbo = std::numeric_limits<unsigned>::max();
	};

	class MeshHandle;

	class Mesh {
	public:
		Mesh(const Mesh& other) = delete;
		Mesh& operator=(const Mesh& other) = delete;

		Mesh(Mesh&& other) noexcept;
		Mesh& operator=(Mesh&& other) noexcept;

		~Mesh();
		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
		Mesh() = default;

		void bindMesh();
		void unbindMesh();
		bool isBinded() { return mBinded; }
		unsigned int getVAO() const { return mData.mVao; }

		SFE::FrustumModule::AABB mBounds;
		Material mMaterial;
		MeshData mData;

		std::vector<MeshHandle*> handles;

		std::vector<ecss::EntityId> loadingEntities;
	private:
		bool mBinded = false;
	};

	class MeshHandle {
	public:
		MeshHandle(const MeshHandle& other)
			: mMaterial(other.mMaterial),
			  mData(other.mData),
			  mBounds(other.mBounds),
			  parentMesh(other.parentMesh),
			  onBind(other.onBind) {
			if (parentMesh) {
				parentMesh->handles.push_back(this);
			}
		}

		MeshHandle& operator=(const MeshHandle& other) {
			if (this == &other)
				return *this;
			mMaterial = other.mMaterial;
			mData = other.mData;
			mBounds = other.mBounds;
			parentMesh = other.parentMesh;
			onBind = other.onBind;
			if (parentMesh) {
				parentMesh->handles.push_back(this);
			}
			return *this;
		}

		MeshHandle(MeshHandle&& other) noexcept
			: mMaterial(other.mMaterial),
			  mData(other.mData),
			  mBounds(other.mBounds),
			  parentMesh(other.parentMesh),
			  onBind(std::move(other.onBind)) {
			if (parentMesh) {
				parentMesh->handles.push_back(this);
			}
		}


		MeshHandle& operator=(MeshHandle&& other) noexcept {
			if (this == &other)
				return *this;
			mMaterial = other.mMaterial;
			mData = other.mData;
			mBounds = other.mBounds;
			parentMesh = other.parentMesh;
			onBind = std::move(other.onBind);
			if (parentMesh) {
				parentMesh->handles.push_back(this);
			}
			return *this;
		}

		MeshHandle() = default;
		~MeshHandle() {
			if (parentMesh) {
				parentMesh->handles.erase(std::find(parentMesh->handles.begin(), parentMesh->handles.end(), this));
			}
		}

		MeshHandle(Mesh& mesh) : mMaterial(&mesh.mMaterial), mData(&mesh.mData), mBounds(&mesh.mBounds), parentMesh(&mesh) {
			mesh.handles.push_back(this);
		}

		const Material* mMaterial = nullptr;
		const MeshData* mData = nullptr;
		const SFE::FrustumModule::AABB* mBounds = nullptr;
		Mesh* parentMesh;

		std::function<void()> onBind = nullptr;
	};
}
