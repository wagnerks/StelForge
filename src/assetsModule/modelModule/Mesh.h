#pragma once

#include <string>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "core/BoundingVolume.h"



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

		unsigned int getVAO() const { return mData.mVao; }

		SFE::FrustumModule::AABB mBounds;
		Material mMaterial;
		MeshData mData;
	private:
		bool mBinded = false;
	};

	class MeshHandle {
	public:
		MeshHandle() = default;

		MeshHandle(const Mesh& mesh) : mMaterial(&mesh.mMaterial), mData(&mesh.mData), mBounds(&mesh.mBounds) {}

		const Material* mMaterial = nullptr;
		const MeshData* mData = nullptr;
		const SFE::FrustumModule::AABB* mBounds = nullptr;

	};
}
