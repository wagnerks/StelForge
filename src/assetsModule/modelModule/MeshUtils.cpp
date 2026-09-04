#include "MeshUtils.h"

#include <cmath>

extern "C" {
#include "mikktspace.h"
}

namespace SFE {
	namespace {
		constexpr float kUvDetEps = 1e-8f;
		constexpr float kLenEps = 1e-12f;

		void forEachTriangle(Mesh<Vertex3D>* mesh, const auto& fn) {
			auto& vertices = mesh->vertices;
			const auto& indices = mesh->indices;
			const auto vertCount = vertices.size();
			if (indices.empty()) {
				for (size_t i = 2; i < vertCount; i += 3) {
					fn(static_cast<unsigned>(i - 2), static_cast<unsigned>(i - 1), static_cast<unsigned>(i));
				}
			}
			else {
				for (size_t i = 2; i < indices.size(); i += 3) {
					const unsigned a = indices[i - 2];
					const unsigned b = indices[i - 1];
					const unsigned c = indices[i];
					if (a >= vertCount || b >= vertCount || c >= vertCount) {
						continue;
					}
					fn(a, b, c);
				}
			}
		}

		void orthonormalizeTB(Vertex3D& v) {
			const float nLen2 = Math::dot(v.normal, v.normal);
			if (nLen2 < kLenEps) {
				v.normal = { 0.f, 1.f, 0.f };
			}
			else {
				v.normal = Math::normalize(v.normal);
			}

			v.tangent = v.tangent - v.normal * Math::dot(v.tangent, v.normal);
			if (Math::dot(v.tangent, v.tangent) < kLenEps) {
				const Math::Vec3 axis = std::fabs(v.normal.y) < 0.99f ? Math::Vec3{ 0.f, 1.f, 0.f } : Math::Vec3{ 1.f, 0.f, 0.f };
				v.tangent = Math::cross(axis, v.normal);
			}
			v.tangent = Math::normalize(v.tangent);

			const Math::Vec3 generatedB = Math::cross(v.normal, v.tangent);
			if (Math::dot(v.biTangent, generatedB) < 0.f) {
				v.biTangent = generatedB * -1.f;
			}
			else {
				v.biTangent = generatedB;
			}
		}

		void accumulateTriangle(Vertex3D& a, Vertex3D& b, Vertex3D& c, bool replace) {
			const auto AB = b.position - a.position;
			const auto AC = c.position - a.position;
			const auto faceN = Math::cross(AB, AC);
			if (Math::dot(faceN, faceN) < kLenEps) {
				return;
			}

			const auto dUv1 = b.texCoords - a.texCoords;
			const auto dUv2 = c.texCoords - a.texCoords;
			const float det = dUv1.x * dUv2.y - dUv2.x * dUv1.y;

			Math::Vec3 faceT{};
			Math::Vec3 faceB{};
			if (std::fabs(det) > kUvDetEps) {
				const float r = 1.f / det;
				faceT = (AB * dUv2.y - AC * dUv1.y) * r;
				faceB = (AC * dUv1.x - AB * dUv2.x) * r;
			}

			auto add = [&](Vertex3D& v) {
				if (replace) {
					v.normal = faceN;
					v.tangent = faceT;
					v.biTangent = faceB;
				}
				else {
					v.normal = v.normal + faceN;
					v.tangent = v.tangent + faceT;
					v.biTangent = v.biTangent + faceB;
				}
			};
			add(a);
			add(b);
			add(c);
		}
	}

	void MeshUtils::recalculateFaceNormal(Vertex3D* a, Vertex3D* b, Vertex3D* c) {
		if (!a || !b || !c) {
			return;
		}
		accumulateTriangle(*a, *b, *c, true);
		orthonormalizeTB(*a);
		orthonormalizeTB(*b);
		orthonormalizeTB(*c);
	}

	void MeshUtils::recalculateFaceNormals(Mesh<Vertex3D>* mesh) {
		if (!mesh || mesh->vertices.empty()) {
			return;
		}
		forEachTriangle(mesh, [&](unsigned ia, unsigned ib, unsigned ic) {
			accumulateTriangle(mesh->vertices[ia], mesh->vertices[ib], mesh->vertices[ic], true);
		});
		for (auto& v : mesh->vertices) {
			orthonormalizeTB(v);
		}
	}

	void MeshUtils::recalculateNormals(Mesh<Vertex3D>* mesh, bool smooth) {
		if (!mesh || mesh->vertices.empty()) {
			return;
		}

		if (!smooth) {
			recalculateFaceNormals(mesh);
			return;
		}

		for (auto& v : mesh->vertices) {
			v.normal = {};
			v.tangent = {};
			v.biTangent = {};
		}
		forEachTriangle(mesh, [&](unsigned ia, unsigned ib, unsigned ic) {
			accumulateTriangle(mesh->vertices[ia], mesh->vertices[ib], mesh->vertices[ic], false);
		});
		for (auto& v : mesh->vertices) {
			orthonormalizeTB(v);
		}
	}

	void MeshUtils::recalculateVerticesNormals(Mesh<Vertex3D>* mesh) {
		recalculateNormals(mesh, true);
	}

	namespace {
		struct MikkMesh {
			Mesh<Vertex3D>* mesh = nullptr;
			bool indexed = false;
			int faces = 0;
		};

		unsigned mikkVert(const MikkMesh* m, int face, int vert) {
			if (m->indexed) {
				return m->mesh->indices[static_cast<size_t>(face) * 3 + static_cast<size_t>(vert)];
			}
			return static_cast<unsigned>(face * 3 + vert);
		}

		int mikkGetNumFaces(const SMikkTSpaceContext* p) {
			return static_cast<const MikkMesh*>(p->m_pUserData)->faces;
		}

		int mikkGetNumVerticesOfFace(const SMikkTSpaceContext*, const int) {
			return 3;
		}

		void mikkGetPosition(const SMikkTSpaceContext* p, float out[], const int face, const int vert) {
			const auto* m = static_cast<const MikkMesh*>(p->m_pUserData);
			const auto& v = m->mesh->vertices[mikkVert(m, face, vert)];
			out[0] = v.position.x;
			out[1] = v.position.y;
			out[2] = v.position.z;
		}

		void mikkGetNormal(const SMikkTSpaceContext* p, float out[], const int face, const int vert) {
			const auto* m = static_cast<const MikkMesh*>(p->m_pUserData);
			const auto& v = m->mesh->vertices[mikkVert(m, face, vert)];
			out[0] = v.normal.x;
			out[1] = v.normal.y;
			out[2] = v.normal.z;
		}

		void mikkGetTexCoord(const SMikkTSpaceContext* p, float out[], const int face, const int vert) {
			const auto* m = static_cast<const MikkMesh*>(p->m_pUserData);
			const auto& v = m->mesh->vertices[mikkVert(m, face, vert)];
			out[0] = v.texCoords.x;
			out[1] = v.texCoords.y;
		}

		void mikkSetTSpaceBasic(const SMikkTSpaceContext* p, const float tangent[], const float sign, const int face, const int vert) {
			auto* m = static_cast<MikkMesh*>(p->m_pUserData);
			auto& v = m->mesh->vertices[mikkVert(m, face, vert)];
			v.tangent = { tangent[0], tangent[1], tangent[2] };
			v.biTangent = Math::cross(v.normal, v.tangent) * sign;
		}
	}

	void MeshUtils::generateTangents(Mesh<Vertex3D>* mesh) {
		if (!mesh || mesh->vertices.empty()) {
			return;
		}

		MikkMesh ctx;
		ctx.mesh = mesh;
		ctx.indexed = !mesh->indices.empty();
		if (ctx.indexed) {
			if (mesh->indices.size() < 3) {
				return;
			}
			ctx.faces = static_cast<int>(mesh->indices.size() / 3);
		}
		else {
			if (mesh->vertices.size() < 3) {
				return;
			}
			ctx.faces = static_cast<int>(mesh->vertices.size() / 3);
		}

		SMikkTSpaceInterface iface{};
		iface.m_getNumFaces = mikkGetNumFaces;
		iface.m_getNumVerticesOfFace = mikkGetNumVerticesOfFace;
		iface.m_getPosition = mikkGetPosition;
		iface.m_getNormal = mikkGetNormal;
		iface.m_getTexCoord = mikkGetTexCoord;
		iface.m_setTSpaceBasic = mikkSetTSpaceBasic;

		SMikkTSpaceContext space{};
		space.m_pInterface = &iface;
		space.m_pUserData = &ctx;
		genTangSpaceDefault(&space);
	}
}
