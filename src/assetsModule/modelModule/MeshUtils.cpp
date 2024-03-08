#include "MeshUtils.h"

#include <map>
#include <vector>

#include "Mesh.h"
#include "mathModule/Forward.h"


namespace SFE {
	void MeshUtils::recalculateFaceNormal(Vertex3D* a, Vertex3D* b, Vertex3D* c) {
		if (!a || !b || !c) {
			return;
		}

		const auto& A = a->position;
		const auto& B = b->position;
		const auto& C = c->position;

		const auto AB = B - A;
		const auto AC = C - A;

		//Normal of ABC triangle
		const auto Normal = SFE::Math::normalize(SFE::Math::cross(AB, AC));

		a->normal = Normal;
		b->normal = Normal;
		c->normal = Normal;

		// Shortcuts for UVs
		const auto& uv0 = a->texCoords;
		const auto& uv1 = b->texCoords;
		const auto& uv2 = c->texCoords;

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

		a->tangent = T;
		b->tangent = T;
		c->tangent = T;

		a->biTangent = biTangent;
		b->biTangent = biTangent;
		c->biTangent = biTangent;
	}

	void MeshUtils::recalculateFaceNormals(Mesh<Vertex3D>* mesh) {
		if (!mesh) {
			return;
		}

		auto& vertices = mesh->vertices;
		if (vertices.empty()) {
			return;
		}

		const auto& indices = mesh->indices;
		if (indices.empty()) {
			for (int i = 2; i < vertices.size(); i += 3) {
				recalculateFaceNormal(&vertices[i - 2], &vertices[i - 1], &vertices[i]);
			}
		}
		else {
			for (int i = 2; i < indices.size(); i += 3) {
				recalculateFaceNormal(&vertices[indices[i - 2]], &vertices[indices[i - 1]], &vertices[indices[i]]);
			}
		}
	}
		

	void MeshUtils::recalculateNormals(Mesh<Vertex3D>* mesh, bool smooth) {
		recalculateFaceNormals(mesh);

		if (smooth) {
			recalculateVerticesNormals(mesh);
		}
	}


	void MeshUtils::recalculateVerticesNormals(Mesh<Vertex3D>* mesh) {
		if (!mesh) {
			return;
		}

		if (mesh->vertices.empty()) {
			return;
		}

		std::map<SFE::Math::Vec3, std::vector<Vertex3D*>> vertices;
		for (auto& vertex : mesh->vertices) {
			vertices[vertex.position].push_back(&vertex);
		}
		recalculateVerticesNormals(vertices);
		
	}

	void MeshUtils::recalculateVerticesNormals(std::map<SFE::Math::Vec3, std::vector<Vertex3D*>>& vertices) {
		std::vector<std::vector<Vertex3D*>> verticesWithOppositTangents;
		verticesWithOppositTangents.emplace_back();
		for (auto [_, copyVertices] : vertices) {
			SFE::Math::Vec3 normal;
			SFE::Math::Vec3 tangent;
			std::vector<Vertex3D*> nears;
			nears.reserve(copyVertices.size());

			while (!copyVertices.empty()) {
				normal = {};
				tangent = {};

				nears.clear();

				if (!verticesWithOppositTangents.back().empty()) {
					verticesWithOppositTangents.emplace_back();
				}

				for (auto vertex : copyVertices) {
					const auto dotProductNormal = Math::dot(vertex->normal, (copyVertices.back())->normal);
					//if dot product <= 0 -> angle >= 90 -> do not sum this normals, to avoid smooth cube, or round sword
					if (std::fabs(dotProductNormal) > std::numeric_limits<float>::epsilon()) {//use back to optimize erasing
						const auto dotProductTangent = Math::dot(vertex->tangent, (copyVertices.back())->tangent);//avoid smoothing opposite tangents
						if (dotProductTangent > std::numeric_limits<float>::epsilon()) {
							nears.push_back(vertex);
							normal += vertex->normal;
							tangent += vertex->tangent;
						}
						verticesWithOppositTangents.back().push_back(vertex);
					}
				}

				if (nears.size() <= 1) { //if nears size == 1 - it means that dot was > 0.f only one time, which possible only if there is no near vertex and algorithm found only copyVertices.back()
					copyVertices.pop_back();
					continue;
				}

				std::erase_if(copyVertices, [nears](Vertex3D* vert) {
					return std::find(nears.begin(), nears.end(), vert) != nears.end();
				});

				normal = normal / nears.size(); //normalize - we know that normal was summed size() times
				tangent = tangent / nears.size();

				tangent = tangent - normal * SFE::Math::dot(tangent, normal);
				auto biTangent = SFE::Math::cross(normal, tangent);//recalculate bitangent to be sure it is perpendicular to normal-tangent plane
				for (auto vertex : nears) {
					vertex->normal = normal;
					vertex->tangent = tangent;
					vertex->biTangent = biTangent;
				}
			}
		}

		for (auto& oppositeVertices : verticesWithOppositTangents) {
			SFE::Math::Vec3 normal;

			for (auto vertex : oppositeVertices) {
				normal += vertex->normal;
			}

			normal = normal / oppositeVertices.size();
			for (auto vertex : oppositeVertices) {
				vertex->normal = normal;
			}
		}
	}
}
