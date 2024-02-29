#include "Mesh.h"

#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "core/ThreadPool.h"
#include "renderModule/Renderer.h"

using namespace AssetsModule;

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<unsigned>& indices) : mData{ std::move(vertices), std::move(indices) } {
	calculateBounds();

	initMeshData();
}

Mesh::~Mesh() {
	releaseMeshData();
}

void Mesh::recalculateFaceNormal(Mesh& mesh, int a, int b, int c) {
	const auto& A = mesh.mData.vertices[a].mPosition;
	const auto& B = mesh.mData.vertices[b].mPosition;
	const auto& C = mesh.mData.vertices[c].mPosition;

	const auto AB = B - A;
	const auto AC = C - A;

	//Normal of ABC triangle
	const auto Normal = SFE::Math::normalize(SFE::Math::cross(AB, AC));

	mesh.mData.vertices[a].mNormal = Normal;
	mesh.mData.vertices[b].mNormal = Normal;
	mesh.mData.vertices[c].mNormal = Normal;

	// Shortcuts for UVs
	const auto& uv0 = mesh.mData.vertices[a].mTexCoords;
	const auto& uv1 = mesh.mData.vertices[b].mTexCoords;
	const auto& uv2 = mesh.mData.vertices[c].mTexCoords;

	// UV delta
	const auto deltaUV1 = uv0 - uv1;
	const auto deltaUV2 = uv0 - uv2;
	const auto divisor = (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
	const float r = divisor ? 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x) : 1.f;

	SFE::Math::Vec3 T = SFE::Math::normalize((AB * deltaUV2.y - AC * deltaUV1.y) * r); //tangent
	// re-orthogonalize T with respect to N
	T = SFE::Math::normalize(T - Normal * SFE::Math::dot(T, Normal));

	// then retrieve perpendicular vector biTangent with the cross product of T and N
	auto biTangent = SFE::Math::normalize(SFE::Math::cross(Normal, T));

	mesh.mData.vertices[a].mTangent = T;
	mesh.mData.vertices[b].mTangent = T;
	mesh.mData.vertices[c].mTangent = T;

	mesh.mData.vertices[a].mBiTangent = biTangent;
	mesh.mData.vertices[b].mBiTangent = biTangent;
	mesh.mData.vertices[c].mBiTangent = biTangent;
}

void Mesh::initMeshData() {
	if (mData.vertices.empty()) {
		return;
	}

	if (SFE::Engine::isMainThread()) {
		releaseMeshData();

		mMeshData.vao.generate();
		mMeshData.vboBuf.generate(SFE::GLW::ARRAY_BUFFER);

		mMeshData.vao.bind();
		mMeshData.vboBuf.bind();
		mMeshData.vboBuf.allocateData(mData.vertices.size(), SFE::GLW::STATIC_DRAW, mData.vertices.data());

		if (!mData.indices.empty()) {
			mMeshData.eboBuf.generate(SFE::GLW::ELEMENT_ARRAY_BUFFER);
			mMeshData.eboBuf.bind();
			mMeshData.eboBuf.allocateData(mData.indices.size(), SFE::GLW::STATIC_DRAW, mData.indices.data());
		}

		mMeshData.vao.addAttribute(0, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex::mPosition);
		mMeshData.vao.addAttribute(1, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex::mNormal);
		mMeshData.vao.addAttribute(2, 2, SFE::GLW::AttributeFType::FLOAT, true, &Vertex::mTexCoords);
		mMeshData.vao.addAttribute(3, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex::mTangent);
		mMeshData.vao.addAttribute(4, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex::mBiTangent);

		mMeshData.vao.addAttribute(5, 4, SFE::GLW::AttributeIType::INT, &Vertex::mBoneIDs);
		mMeshData.vao.addAttribute(6, 4, SFE::GLW::AttributeFType::FLOAT, false, &Vertex::mWeights);
		mMeshData.vao.bindDefault();

	}
	else {
		SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([this]()mutable { //easy crash 
			initMeshData();
		});
	}
}

void Mesh::releaseMeshData() {
	if (!isBinded()) {
		return;
	}
	
	if (SFE::Engine::isMainThread()) {
		mMeshData.release();
	}
	else {
		auto data = new MeshData(std::move(mMeshData));
		SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([data]()mutable {
			delete data;
		});
	}
}

void Mesh::recalculateNormals(bool smooth) {
	recalculateFaceNormals();

	if (smooth) {
		recalculateVerticesNormals();
	}

	initMeshData();
}

void Mesh::calculateBounds() {
	if (mData.vertices.empty()) {
		return;
	}


	auto minAABB = SFE::Math::Vec3(std::numeric_limits<float>::max());
	auto maxAABB = SFE::Math::Vec3(std::numeric_limits<float>::min());

	for (auto vertex : mData.vertices) {
		vertex.mPosition = transform * SFE::Math::Vec4(vertex.mPosition, 1.f);
		minAABB.x = std::min(minAABB.x, vertex.mPosition.x);
		minAABB.y = std::min(minAABB.y, vertex.mPosition.y);
		minAABB.z = std::min(minAABB.z, vertex.mPosition.z);

		maxAABB.x = std::max(maxAABB.x, vertex.mPosition.x);
		maxAABB.y = std::max(maxAABB.y, vertex.mPosition.y);
		maxAABB.z = std::max(maxAABB.z, vertex.mPosition.z);
	}

	mBounds = SFE::FrustumModule::AABB(minAABB, maxAABB);
}

void Mesh::recalculateFaceNormals() {
	if (mData.vertices.empty()) {
		return;
	}

	for (int i = 2; i < mData.indices.size(); i += 3) {
		recalculateFaceNormal(*this, mData.indices[i - 2], mData.indices[i - 1], mData.indices[i]);
	}
}

void Mesh::recalculateVerticesNormals() {
	if (mData.vertices.empty()) {
		return;
	}

	std::map<SFE::Math::Vec3, std::vector<Vertex*>> vertices;
	for (auto& vertex : mData.vertices) {
		vertices[vertex.mPosition].push_back(&vertex);
	}

	std::vector<std::vector<Vertex*>> verticesWithOppositTangents;
	verticesWithOppositTangents.emplace_back();
	for (auto [_, copyVertices] : vertices) {
		SFE::Math::Vec3 normal;
		SFE::Math::Vec3 tangent;
		std::vector<Vertex*> nears;
		nears.reserve(copyVertices.size());

		while (!copyVertices.empty()) {
			normal = {};
			tangent = {};

			nears.clear();

			if (!verticesWithOppositTangents.back().empty()) {
				verticesWithOppositTangents.emplace_back();
			}

			for (auto vertex : copyVertices) {
				const auto dotProductNormal = dot(vertex->mNormal, (copyVertices.back())->mNormal);
				//if dot product <= 0 -> angle >= 90 -> do not sum this normals, to avoid smooth cube, or round sword
				if (std::fabs(dotProductNormal) > std::numeric_limits<float>::epsilon()) {//use back to optimize erasing
					const auto dotProductTangent = dot(vertex->mTangent, (copyVertices.back())->mTangent);//avoid smoothing opposite tangents
					if (dotProductTangent > std::numeric_limits<float>::epsilon()) {
						nears.push_back(vertex);
						normal += vertex->mNormal;
						tangent += vertex->mTangent;
					}
					verticesWithOppositTangents.back().push_back(vertex);
				}
			}

			if (nears.size() <= 1) { //if nears size == 1 - it means that dot was > 0.f only one time, which possible only if there is no near vertex and algorithm found only copyVertices.back()
				copyVertices.pop_back();
				continue;
			}

			std::erase_if(copyVertices, [nears](Vertex* vert) {
				return std::find(nears.begin(), nears.end(), vert) != nears.end();
			});

			normal = normal / nears.size(); //normalize - we know that normal was summed size() times
			tangent = tangent / nears.size();

			tangent = tangent - normal * SFE::Math::dot(tangent, normal);
			auto biTangent = SFE::Math::cross(normal, tangent);//recalculate bitangent to be sure it is perpendicular to normal-tangent plane
			for (auto vertex : nears) {
				vertex->mNormal = normal;
				vertex->mTangent = tangent;
				vertex->mBiTangent = biTangent;
			}
		}
	}

	for (auto& oppositeVertices : verticesWithOppositTangents) {
		SFE::Math::Vec3 normal;

		for (auto vertex : oppositeVertices) {
			normal += vertex->mNormal;
		}

		normal = normal / oppositeVertices.size();
		for (auto vertex : oppositeVertices) {
			vertex->mNormal = normal;
		}
	}
}
