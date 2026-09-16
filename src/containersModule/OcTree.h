#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <numbers>
#include <vector>
#include <map>

#include "mathModule/Forward.h"
#include "myPhysicsEngine/Physics.h"
#include "renderModule/Utils.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "multithreading/ThreadPool.h"

namespace SFE {
	enum Octant : uint8_t {
		LTN = 0,
		RTN = 1,
		LBN = 2,
		RBN = 3,

		LTF = 4,
		RTF = 5,
		LBF = 6,
		RBF = 7
	};

	inline static bool isPointInBox(const SFE::Math::Vec3& pos, const SFE::Math::Vec3& nodePos, float nodeSize) {
		return (pos.x >= nodePos.x && pos.x <= nodePos.x + nodeSize)
			&& (pos.y <= nodePos.y && pos.y >= nodePos.y - nodeSize)
			&& (pos.z >= nodePos.z && pos.z <= nodePos.z + nodeSize);
	}

	inline static Math::Vec3 getOctantPos(Octant boxOctant, float nodeSize) {
		constexpr SFE::Math::Vec3 kOctantUnit[8] = {
			{0,  0,  1},  // LTN
			{1,  0,  1},  // RTN
			{0, -1,  1},  // LBN
			{1, -1,  1},  // RBN
			{0,  0,  0},  // LTF
			{1,  0,  0},  // RTF
			{0, -1,  0},  // LBF
			{1, -1,  0},  // RBF
		};

		return kOctantUnit[static_cast<uint8_t>(boxOctant)] * nodeSize;
	}

	// Calls func with the corner of every octreeSize grid cell the box overlaps. Cell i spans [i, i + 1) * octreeSize
	// on each axis; a tree is keyed by its left-top-far corner: min x, max y, min z. Boxes over more than
	// maxCellsPerAxis cells on an axis, or with non-finite bounds, are skipped.
	template<class Func>
	inline void forEachOctreeCell(const FrustumModule::AABB& aabb, float octreeSize, Func&& func, int maxCellsPerAxis = 8) {
		const Math::Vec3 lo = (aabb.center - aabb.extents) / octreeSize;
		const Math::Vec3 hi = (aabb.center + aabb.extents) / octreeSize;
		Math::Vec3 first;
		Math::Vec3 last;
		for (size_t axis = 0; axis < 3; ++axis) {
			if (!std::isfinite(lo[axis]) || !std::isfinite(hi[axis]) || hi[axis] - lo[axis] > static_cast<float>(maxCellsPerAxis)) {
				return;
			}
			first[axis] = std::floor(lo[axis]);
			// a box ending exactly on a boundary does not reach the next cell
			last[axis] = (std::max)(first[axis], std::ceil(hi[axis]) - 1.f);
		}

		for (float x = first.x; x <= last.x; ++x) {
			for (float y = first.y; y <= last.y; ++y) {
				for (float z = first.z; z <= last.z; ++z) {
					std::invoke(func, Math::Vec3{ x * octreeSize, (y + 1.f) * octreeSize, z * octreeSize });
				}
			}
		}
	}

	template<typename DataType, size_t Deep = 4, size_t Size = 256>
	struct OcTree : public ThreadSynchronizer {
		struct Object {
			friend bool operator<(const Object& lhs, const Object& rhs) { return lhs.data < rhs.data; }
			friend bool operator<=(const Object& lhs, const Object& rhs) { return !(rhs < lhs); }
			friend bool operator>(const Object& lhs, const Object& rhs) { return rhs < lhs; }
			friend bool operator>=(const Object& lhs, const Object& rhs) { return !(lhs < rhs); }
			friend bool operator==(const Object& lhs, const Object& rhs) { return lhs.data == rhs.data; }
			friend bool operator==(const Object& lhs, const DataType& rhs) { return lhs.data == rhs; }

			bool operator==(const Object& rhs) { return operator==(*this, rhs); }
			bool operator==(const DataType& rhs) { return operator==(*this, rhs); }
			bool operator!=(const Object& rhs) { return !(operator==(rhs)); }
			bool operator!=(const DataType& rhs) { return !(operator==(rhs)); }

			Object(const Math::Vec3& pos, const Math::Vec3& size, const DataType& data) : pos(pos), size(size), data(data) {}
			Object() = default;

			Math::Vec3 pos;
			Math::Vec3 size;
			DataType data;
		};

		struct Node { std::vector<Object> data; };

	public:
		OcTree(const Math::Vec3& octreePos) : mPos(octreePos) {}
		OcTree() = default;

	public:
		static inline Math::Vec3 calculateNodeCenter(const Math::Vec3& ltfPos, float size) { return ltfPos + Math::Vec3(size, -size, size) * 0.5f; }

		static inline bool isAABBInBoxAny(const Math::Vec3& pos, const Math::Vec3& size, const Math::Vec3& nodePos, float nodeSize) {
			
			// Convert object AABB (center + half-extents) to min/max
			const Math::Vec3 objMin = pos - size;
			const Math::Vec3 objMax = pos + size;

			// Convert node cube (corner + size) to min/max
			// Note: Y axis is inverted (nodePos.y is top, nodePos.y - nodeSize is bottom)
			const Math::Vec3 nodeMin(nodePos.x, nodePos.y - nodeSize, nodePos.z);
			const Math::Vec3 nodeMax(nodePos.x + nodeSize, nodePos.y, nodePos.z + nodeSize);

			// Fast AABB overlap test (separating axis theorem)
			return (objMin.x <= nodeMax.x && objMax.x >= nodeMin.x)
				&& (objMin.y <= nodeMax.y && objMax.y >= nodeMin.y)
				&& (objMin.z <= nodeMax.z && objMax.z >= nodeMin.z);
		}

		static inline bool isAABBInBox(const Math::Vec3& pos, const Math::Vec3& size, const Math::Vec3& nodePos, float nodeSize) {
			const Math::Vec3 objMin = pos - size;
			const Math::Vec3 objMax = pos + size;

			const Math::Vec3 nodeMin(nodePos.x, nodePos.y - nodeSize, nodePos.z);
			const Math::Vec3 nodeMax(nodePos.x + nodeSize, nodePos.y, nodePos.z + nodeSize);
			if (objMin.x < nodeMin.x || objMax.x > nodeMax.x) return false;
			if (objMin.z < nodeMin.z || objMax.z > nodeMax.z) return false;
			if (objMin.y < nodeMin.y || objMax.y > nodeMax.y) return false;
			return true;
		}

	public:
		inline void drawOctree(bool drawObjAABB) {
			constexpr static Math::Mat4 rotate = {
				{1.f,0.f,0.f,0.f},
				{0.f,1.f,0.f,0.f},
				{0.f,0.f,1.f,0.f},
				{0.f,0.f,0.f,1.f}
			};


			constexpr static auto notEmptyColor = Math::Vec4(0.f, 0.8f, 0.f, 0.12f);

			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node)
			{
				if (node.data.size()) {
					Render::Utils::renderCubeMesh(
						Math::Vec3(0.f, 0.f, nodeSize),
						Math::Vec3(nodeSize, -nodeSize, 0.f),
						rotate, Math::Vec3(nodePos), notEmptyColor
					);

					if (drawObjAABB) {
						for (auto& data : node.data) {
							constexpr static auto AABBColor = Math::Vec4(0.f, 1.f, 1.f, 1.f);

							Render::Utils::renderCube(
								Math::Vec3(-data.size.x, -data.size.y, data.size.z),
								Math::Vec3(data.size.x, data.size.y, -data.size.z),
								rotate, Math::Vec3(data.pos), AABBColor
							);
						}
					}
					constexpr static auto emptyColor = Math::Vec4(1.f, 0.f, 1.f, 1.f);

					Render::Utils::renderCube(
						Math::Vec3(0.f, 0.f, nodeSize),
						Math::Vec3(nodeSize, -nodeSize, 0.f),
						rotate, Math::Vec3(nodePos), emptyColor
					);
				}
			});
		}

	public:
		struct StackEntry {
			SFE::Math::Vec3 pos;
			float size;
			size_t index;
			uint8_t frustumFaces = FrustumModule::kAllFrustumFaces; // faces the node still crosses
		};

		template<class Func>
		inline void forEachObjectInsideFrustum(const SFE::FrustumModule::Frustum& frustum, Func&& func) {
			if (nodes.empty()) {
				return;
			}

			std::array<StackEntry, 1 + (8 - 1) * (Deep - 1)> stack;
			size_t top = 0;
			stack[top++] = { mPos, mSize, 0u };

			while (top != 0) {
				const auto entry = stack[--top];
				auto faces = entry.frustumFaces;
				if (faces && !FrustumModule::cullFrustumFaces(frustum, calculateNodeCenter(entry.pos, entry.size), SFE::Math::Vec3{ entry.size * 0.5f }, faces)) {
					continue;
				}

				// The root also keeps objects that only overlap it, so its objects are tested against every face;
				// objects below the root lie inside their node and only need the faces the node still crosses.
				const auto objectFaces = entry.index == 0 ? FrustumModule::kAllFrustumFaces : faces;
				for (const auto& object : nodes[entry.index].data) {
					auto objectFacesLeft = objectFaces;
					if (!objectFacesLeft || FrustumModule::cullFrustumFaces(frustum, object.pos, object.size, objectFacesLeft)) {
						std::invoke(std::forward<Func>(func), object.data);
					}
				}

				auto firstChildId = entry.index * 8 + 1;
				if (firstChildId + 7 >= NodesCount) {
					continue;
				}

				const auto childSize = entry.size * 0.5f;
				for (auto octant = 0u; octant < 8; octant++) {
					stack[top].pos = entry.pos + getOctantPos(static_cast<Octant>(octant), childSize);
					stack[top].size = childSize;
					stack[top].index = firstChildId + octant;
					stack[top].frustumFaces = faces;
					++top;
				}
			}
		}

		template<class Func>
		inline void forEachNode(Func&& func)
		{
			if (nodes.empty()) {
				return;
			}

			std::array<StackEntry, 1 + (8 - 1) * (Deep - 1)> stack;
			size_t top = 0;
			stack[top++] = { mPos, mSize, 0u };

			while (top != 0) {
				const auto entry = stack[--top];
				auto& node = nodes[entry.index];

				std::invoke(func, entry.pos, entry.size, node);

				auto firstChildId = entry.index * 8 + 1;
				if (firstChildId + 7 >= NodesCount) {
					continue;
				}

				const auto childSize = entry.size * 0.5f;
				for (auto octant = 0u; octant < 8; ++octant) {
					stack[top].pos = entry.pos + getOctantPos(static_cast<Octant>(octant), childSize);
					stack[top].size = childSize;
					stack[top].index = firstChildId + octant;
					++top;
				}
			}
		}

		template<class Func, class Pred>
		inline void forEachNode(Func&& func, Pred&& pred) {
			if (nodes.empty()) {
				return;
			}

			std::array<StackEntry, 1 + (8 - 1) * (Deep - 1)> stack;
			size_t top = 0;
			stack[top++] = { mPos, mSize, 0u };

			while (top != 0) {
				const auto entry = stack[--top];
				auto& node = nodes[entry.index];
				if (std::invoke(pred, entry.pos, entry.size, node)) {
					std::invoke(func, entry.pos, entry.size, node);

					auto firstChildId = entry.index * 8 + 1;
					if (firstChildId + 7 >= NodesCount) {
						continue;
					}

					const auto childSize = entry.size * 0.5f;
					for (auto octant = 0u; octant < 8; ++octant) {
						stack[top].pos = entry.pos + getOctantPos(static_cast<Octant>(octant), childSize);
						stack[top].size = childSize;
						stack[top].index = firstChildId + octant;
						++top;
					}
				}
			}
		}

		template<class Func, class Pred>
		inline void forEachObject(Func&& func, Pred&& pred) {
			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node)
			{
				for (const auto& object : node.data) {
					std::invoke(std::forward<Func>(func), object);
				}
			}, std::forward<Pred>(pred));
		}

		template<class Func>
		inline void forEach(Func&& func) {
			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node)
			{
				for (auto& object : node.data) {
					std::invoke(std::forward<Func>(func), object);
				}
			});
		}

		template<class Func>
		inline void forEach(Func&& func) const {
			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node)
			{
				for (const auto& object : node.data) {
					std::invoke(std::forward<Func>(func), object);
				}
			});
		}

	public:
		void clear() {
			for (auto& node : nodes) {
				node.data.clear();
			}
		}

		void erase(const DataType& data, size_t nodeIndex = static_cast<size_t>(-1)) {
			if (nodeIndex < NodesCount) {
				auto& bucket = nodes[nodeIndex].data;
				bucket.erase(std::remove_if(bucket.begin(), bucket.end(), [&](const auto& a) { return a == data; }), bucket.end());
				return;
			}
			for (auto& node : nodes) {
				node.data.erase(std::remove_if(node.data.begin(), node.data.end(), [&](const auto& a) { return a == data; }), node.data.end());
			}
		}

		bool insert(const SFE::Math::Vec3& globalPos, const SFE::Math::Vec3& size, const DataType& data, size_t* outNode = nullptr) {
			if (!isAABBInBoxAny(globalPos, size, mPos, mSize)) {
				return false;
			}

			size_t index = 0;
			auto nodePos = mPos;
			auto nodeSize = mSize;

			for (size_t depth = 0; depth + 1 < Deep; ++depth) {
				auto childSize = nodeSize * 0.5f;
				bool descended = false;

				for (auto octant = 0; octant < 8; octant++) 	{
					auto childPos = nodePos + getOctantPos(static_cast<Octant>(octant), childSize);

					if (isAABBInBox(globalPos, size, childPos, childSize)) {
						index = index * 8 + 1 + octant;
						nodePos = childPos;
						nodeSize = childSize;
						descended = true;
						break;
					}
				}

				if (!descended) {
					break;
				}
			}

			nodes[index].data.emplace_back(globalPos, size, data);
			if (outNode) {
				*outNode = index;
			}
			return true;
		}

	public:
		std::vector<std::pair<SFE::Math::Vec3, Object>> findCollisions(const SFE::Math::Vec3& src, const Math::Vec3& dir, const std::function<bool(const Object& data)>& pred = nullptr) {
			std::vector<std::pair<SFE::Math::Vec3, Object>> collisions;
			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node) {
				const auto collisionRes = 
					PhysicsEngine::Physics::checkCollision({ calculateNodeCenter(nodePos, nodeSize), Math::Vec3{ nodeSize * 0.5f } }, src, dir);

				if (!collisionRes.second) {
					return;
				}

				for (const auto& nodeData : node.data) {
					auto cube = PhysicsEngine::Cube{ nodeData.pos, nodeData.size };
					auto collision = PhysicsEngine::Physics::checkCollision(cube, src, dir, true);

					if (collision.second && isPointInBox(collision.first, mPos, mSize) && (pred ? pred(nodeData) : true)) {
						collisions.emplace_back(collision.first, nodeData);
					}
				}
			});


			std::sort(collisions.begin(), collisions.end(), [src](const std::pair<SFE::Math::Vec3, Object>& a, const std::pair<SFE::Math::Vec3, Object>& b) {
				return SFE::Math::lengthSquared(src - a.first) < SFE::Math::lengthSquared(src - b.first);
			});

			return collisions;
		}

	public:
//		LTF*------------*RTF
//		 / |           /|
//      /  |          / |
//     /   |         /  |
// LTN*-----------*RTN  |
//    |    |        |   |
//    |    |        |   |
//    |    *LBF-----|---*RBF
//    |   /         |  /
//    |  /          | /
//    | /           |/
// LBN*-------------*RBN

		template <size_t D>
		static consteval size_t OctreeNodeCount() {
			size_t count = 0;
			size_t pow = 1;
			for (size_t i = 0; i < D; ++i) {
				count += pow;   // 8^i
				pow *= 8;
			}
			return count;
		}

		static constexpr size_t NodesCount = OctreeNodeCount<Deep>();

		// [0] - root
		// [1..8] - depth 1
		// [9..72] - depth 2
		// [73..584] - depth 3
		// ...
		std::array<Node, NodesCount> nodes{};

		Math::Vec3 mPos; //LTF left top far point //todo instead LTF use LBF
		float mSize = static_cast<float>(Size);
	};
}
