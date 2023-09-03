#pragma once
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "ecsModule/EntityBase.h"
#include "renderModule/TextureHandler.h"


namespace Engine {
	namespace FrustumModule {
		struct AABB;
	}
}

namespace Engine::ModelModule {
	struct Vertex {
		glm::vec3 mPosition;
		glm::vec3 mNormal;
		glm::vec2 mTexCoords;
		glm::vec3 mTangent;
	};

	struct MaterialTexture {
		RenderModule::Texture mTexture;
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

		unsigned int getVAO() const { return mData.mVao;}

		FrustumModule::AABB* mBounds = nullptr;
		Material mMaterial;
		MeshData mData;
	private:
		bool mBinded = false;
	};

	class MeshHandle {
	public:
		MeshHandle() = default;

		MeshHandle(const Mesh& mesh) : mMaterial(mesh.mMaterial), mData(mesh.mData), mBounds(mesh.mBounds) {}

		Material mMaterial;
		MeshData mData;
		FrustumModule::AABB* mBounds = nullptr;

	};
}
