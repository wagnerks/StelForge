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

	enum CardinalDirection : uint8_t {
		LEFT_TOP_NEAR = 0,
		RIGHT_TOP_NEAR = 1,

		LEFT_BTM_NEAR = 2,
		RIGHT_BTM_NEAR = 3,

		LEFT_TOP_FAR = 4,
		RIGHT_TOP_FAR = 5,

		LEFT_BTM_FAR = 6,
		RIGHT_BTM_FAR = 7,

		LEFT = 8,
		RIGHT = 9,
		TOP = 10,
		BOTTOM = 11,
		FRONT = 12,
		BACK = 13,

		LEFT_TOP,
		RIGHT_TOP,

		LEFT_BTM,
		RIGHT_BTM,

		LEFT_NEAR,
		RIGHT_NEAR,

		LEFT_FAR,
		RIGHT_FAR,

		TOP_NEAR,
		BTM_NEAR,

		TOP_FAR,
		BTM_FAR
	};

	constexpr std::array<std::array<size_t, 8>, 26> traversal {
	{
			{ LTN, LTF, RTN, LBN, RTF, LBF, RBN, RBF },
			{ RTN, LTN, RTF, RBN, LTF, LBN, RBF, LBF },
			{ LBN, LBF, RBN, LTN, RBF, LTF, RTN, RTF },
			{ RBN, LBN, RBF, RTN, LBF, LTN, RTF, LTF },

			{ LTF, LTN, RTF, LBF, RTN, LBN, RBF, RBN },
			{ RTF, LTF, RTN, RBF, LTN, RBN, LBF, LBN},
			{ LBF, LBN, RBF, LTF, RBN, LTN, RTF, RTN },
			{ RBF, RBN, LBF, RTF, LBN, RTN, LTF, LTN },

			{ LBF, LBN, LTF, LTN, RTF, RTN, RBF, RBN },
			{ RBN, RBF, RTN, RTF, LBN, LBF, LTN, LTF },

			{ LTN, LTF, RTF, RTN, LBN, RBN, LBF, RBF },
			{ LBN, LBF, RBF, RBN, LTN, LTF, RTF, RTN },

			{ LBN, RBN, LTN, RTN, LBF, LTF, RTF, RBF },
			{ LBF, LTF, RTF, RBF, LBN, LTN, RTN, RBN },

			{ LTF, LTN, RTF, RTN, LBF, LBN, RBN, RBF },
			{ RTF, RTN, LTF, LTN, RBN, RBF, LBF, LBN },

			{ LBF, LBN, RBN, RBF, LTF, LTN, RTF, RTN },
			{ RBN, RBF, LBN, LBF, RTF, RTN, LTF, LTN },

			{ LTN, LBN, RTN, RBN, LBF, LTF, RTF, RBF },
			{ RTN, RBN, LBN, LTN, RTF, RBF, LTF, LBF },

			{ LBF, LTF, LTN, LBN, RTF, RBF, RTN, RBN },
			{ RTF, RBF, RTN, RBN, LTF, LBF, LTN, LBN },

			{ LTN, RTN, LTF, RTF, LBN, RBN, LBF, RBF },
			{ LBN, RBN, LTN, RTN, LBF, RBF, LTF, RTF},

			{ LTF, RTF, LBF, RBF, LTN, RTN, LBN, RBN },
			{ LBF, RBF, LTF, RTF, LBN, RBN, LTN, RTN }
		}
	};

	inline static bool isPointInBox(const SFE::Math::Vec3& pos, const SFE::Math::Vec3& nodePos, float nodeSize) {
		return (pos.x >= nodePos.x && pos.x <= nodePos.x + nodeSize)
			&& (pos.y <= nodePos.y && pos.y >= nodePos.y - nodeSize)
			&& (pos.z >= nodePos.z && pos.z <= nodePos.z + nodeSize);
	}

	inline static bool isBoxInBox(const SFE::Math::Vec3& pos, const SFE::Math::Vec3& nodePos, const SFE::Math::Vec3& nodeSize) {
		return (pos.x >= nodePos.x - nodeSize.x && pos.x <= nodePos.x + nodeSize.x)
			&& (pos.y <= nodePos.y + nodeSize.y && pos.y >= nodePos.y - nodeSize.y)
			&& (pos.z >= nodePos.z - nodeSize.z && pos.z <= nodePos.z + nodeSize.z);
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

	inline static void calculateAABBOctrees(const FrustumModule::AABB& aabb, float octreeSize, float& maxX, float& minX, float& maxY, float& minY, float& maxZ, float& minZ) {
		maxX = std::round(aabb.center.x + aabb.extents.x);
		minX = std::round(aabb.center.x - aabb.extents.x);

		maxY = std::round(aabb.center.y + aabb.extents.y);
		minY = std::round(aabb.center.y - aabb.extents.y);

		maxZ = std::round(aabb.center.z + aabb.extents.z);
		minZ = std::round(aabb.center.z - aabb.extents.z);

		auto remainder = [octreeSize](float val) {
			return static_cast<int>(std::fabs(val)) % static_cast<int>(octreeSize);
		};

		auto calcMinus = [&remainder, octreeSize](float val) {
			if (val < 0.f) {
				const auto remain = remainder(val);
				if (remain) {
					return val + remain - octreeSize;
				}
				return val;
			}
			else if (val > 0.f) {
				return val - remainder(val);
			}

			return 0.f;
		};

		auto calcPlus = [&remainder, octreeSize](float val) {
			if (val < 0.f) {
				return val + remainder(val);
			}
			else if (val > 0.f) {
				const auto remain = remainder(val);
				if (remain) {
					return val - remain + octreeSize;
				}
				return val;
			}

			return 0.f;
		};

		maxX = calcPlus(maxX);
		minX = calcMinus(minX);

		maxY = calcPlus(maxY);
		minY = calcMinus(minY);

		maxZ = calcPlus(maxZ);
		minZ = calcMinus(minZ);
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

		static inline CardinalDirection getSourceCardinalDirection(const SFE::Math::Vec3& nodePos, float nodeSize, const SFE::Math::Vec3& srcPoint) {
			bool left = false;
			bool xMid = false;
			if (srcPoint.x >= nodePos.x && srcPoint.x <= nodePos.x + nodeSize) {
				xMid = true;
			}
			else if (srcPoint.x < nodePos.x) {
				left = true;
			}

			bool top = false;
			bool yMid = false;
			if (srcPoint.y <= nodePos.y && srcPoint.y >= nodePos.y - nodeSize) {
				yMid = true;
			}
			else if (srcPoint.y > nodePos.y) {
				top = true;
			}

			bool far = false;
			bool zMid = false;
			if (srcPoint.z > nodePos.z && srcPoint.z < nodePos.z + nodeSize) {
				zMid = true;
			}
			else if (srcPoint.z < nodePos.z) {
				far = true;
			}

			if (left) {
				if (top) {
					if (far) {
						return LEFT_TOP_FAR;
					}
					else if (zMid) {
						return LEFT_TOP;
					}
					else {
						return LEFT_TOP_NEAR;
					}
				}
				else if (yMid) {
					if (far) {
						return LEFT_FAR;
					}
					else if (zMid) {
						return LEFT;
					}
					else {
						return LEFT_NEAR;
					}
				}
				else {
					if (far) {
						return LEFT_BTM_FAR;
					}
					else if (zMid) {
						return LEFT_BTM;
					}
					else {
						return LEFT_BTM_NEAR;
					}
				}
			}
			else if (xMid) {
				if (top) {
					if (far) {
						return TOP_FAR;
					}
					else if (zMid) {
						return TOP;
					}
					else {
						return TOP_NEAR;
					}
				}
				else if (yMid) {
					if (far) {
						return FRONT;
					}
					else if (zMid) {
						const auto size = nodeSize * 0.5f;
						const bool isLeft = srcPoint.x < nodePos.x + size;
						const bool isTop = srcPoint.y > nodePos.y - size;
						const bool isFar = srcPoint.z < nodePos.z + size;
						if (isLeft) {
							if (isTop) {
								if (isFar) {
									return LEFT_TOP_FAR;
								}
								else {
									return LEFT_TOP_NEAR;
								}
							}
							else {
								if (isFar) {
									return LEFT_BTM_FAR;
								}
								else {
									return LEFT_BTM_NEAR;
								}
							}
						}
						else {
							if (isTop) {
								if (isFar) {
									return RIGHT_TOP_FAR;
								}
								else {
									return RIGHT_TOP_NEAR;
								}
							}
							else {
								if (isFar) {
									return RIGHT_BTM_FAR;
								}
								else {
									return RIGHT_BTM_NEAR;
								}
							}
						}
					}
					else {
						return BACK;
					}
				}
				else {
					if (far) {
						return BTM_FAR;
					}
					else if (zMid) {
						return BOTTOM;
					}
					else {
						return BTM_NEAR;
					}
				}
			}
			else {
				if (top) {
					if (far) {
						return RIGHT_TOP_FAR;
					}
					else if (zMid) {
						return RIGHT_TOP;
					}
					else {
						return RIGHT_TOP_NEAR;
					}
				}
				else if (yMid) {
					if (far) {
						return RIGHT_FAR;
					}
					else if (zMid) {
						return RIGHT;
					}
					else {
						return RIGHT_NEAR;
					}
				}
				else {
					if (far) {
						return RIGHT_BTM_FAR;
					}
					else if (zMid) {
						return RIGHT_BTM;
					}
					else {
						return RIGHT_BTM_NEAR;
					}
				}
			}

			return LEFT_TOP_NEAR;
		}

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


			constexpr static auto notEmptyColor = Math::Vec4(0.f, 0.5f, 0.f, 0.02f);

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
				auto center = calculateNodeCenter(entry.pos, entry.size);
				auto halfSize = entry.size * 0.5f;
				bool entirely = FrustumModule::SquareAABB::isOnFrustumEntirely(frustum, center, halfSize);
				if (entirely || FrustumModule::SquareAABB::isOnFrustum(frustum, center, halfSize)) {
					auto& node = nodes[entry.index];
					for (const auto& object : node.data) {
						if (entirely || FrustumModule::AABB::isOnFrustum(frustum, object.pos, object.size)) {
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
						++top;
					}
				}
			}
		}

		template<class Func>
		inline void forEachNode(Func&& func, CardinalDirection traversalDirection = static_cast<CardinalDirection>(0))
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
				for (auto octant : traversal[traversalDirection]) {
					stack[top].pos = entry.pos + getOctantPos(static_cast<Octant>(octant), childSize);
					stack[top].size = childSize;
					stack[top].index = firstChildId + octant;
					++top;
				}
			}
		}

		template<class Func, class Pred>
		inline void forEachNode(Func&& func, Pred&& pred, CardinalDirection traversalDirection = static_cast<CardinalDirection>(0)) {
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
					for (auto octant : traversal[traversalDirection]) {
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

		void erase(const DataType& data) {
			for (auto& node : nodes) {
				node.data.erase(std::remove_if(node.data.begin(), node.data.end(), [&](const auto& a) { return a == data; }), node.data.end());
			}
		}

		bool insert(const SFE::Math::Vec3& globalPos, const SFE::Math::Vec3& size, const DataType& data) {
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
			return true;
		}

	public:
		std::vector<std::pair<SFE::Math::Vec3, Object>> findCollisions(const SFE::Math::Vec3& src, const Math::Vec3& dir, const std::function<bool(const Object& data)>& pred = nullptr) {
			const auto octant = getSourceCardinalDirection(mPos, mSize, src);

			std::vector<std::pair<SFE::Math::Vec3, Object>> collisions;
			forEachNode([&](const SFE::Math::Vec3& nodePos, float nodeSize, Node& node) {
				const auto collisionRes = 
					PhysicsEngine::Physics::checkCollision({ { nodePos.x + nodeSize, nodePos.y - nodeSize, nodePos.z + nodeSize }, {nodeSize} }, src, dir);

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
			}, octant);


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
