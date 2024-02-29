#pragma once

#include <functional>
#include <string>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "core/BoundingVolume.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"
#include "mathModule/Forward.h"

namespace AssetsModule {
#define MAX_BONE_INFLUENCE 4

	struct Vertex {
		SFE::Math::Vec3 mPosition;
		SFE::Math::Vec3 mNormal;
		SFE::Math::Vec2 mTexCoords;
		SFE::Math::Vec3 mTangent;
		SFE::Math::Vec3 mBiTangent;

		int mBoneIDs[MAX_BONE_INFLUENCE]{-1,-1,-1,-1};
		float mWeights[MAX_BONE_INFLUENCE]{0.f,0.f,0.f,0.f};
	};

	enum MaterialType : uint8_t {
		DIFFUSE = 1,
		NORMALS,
		SPECULAR
	};
	
	struct Material {
		Texture*& operator[](MaterialType type) { return materialTextures[type]; }
		Texture* tryGetTexture(MaterialType type) const {
			const auto it = materialTextures.find(type);

			if (it != materialTextures.cend()) {
				return it->second;
			}

			return nullptr;
		}

	private:
		std::unordered_map<MaterialType, Texture*> materialTextures;
	};

	struct MeshVertices {
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
	};


	struct MeshData {
		SFE::GLW::VertexArray vao;
		SFE::GLW::Buffer vboBuf;
		SFE::GLW::Buffer eboBuf;

		void release() {
			vao.release();
			vboBuf.release();
			eboBuf.release();
		}
	};

	class Mesh {
	public:
		Mesh(const Mesh& other) = delete;
		Mesh(Mesh&& other) noexcept = default;
		Mesh& operator=(const Mesh& other) = delete;
		Mesh& operator=(Mesh&& other) noexcept = default;

	public:
		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
		Mesh() = default;
		~Mesh();

		void initMeshData();
		void releaseMeshData();

		bool isBinded() const { return mMeshData.vao; }
		unsigned int getVAO() const { return mMeshData.vao.getID(); }

		const SFE::FrustumModule::AABB& getBounds() const { return mBounds; }

		void calculateBounds();
		void recalculateNormals(bool smooth);

	public:
		Material mMaterial;
		MeshVertices mData;

		SFE::Math::Mat4 transform;
		uint8_t lod = 0;

	private:
		SFE::FrustumModule::AABB mBounds;

		void recalculateFaceNormals();
		void recalculateVerticesNormals();

		MeshData mMeshData;

	public:
		static void recalculateFaceNormal(Mesh& mesh, int a, int b, int c);
	};

}
