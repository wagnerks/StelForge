#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <vector>
#include <map>

#include "mathModule/Forward.h"
#include "myPhysicsEngine/Physics.h"
#include "renderModule/Utils.h"
#include "core/BoundingVolume.h"
#include "ThreadPool.h"

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
		if (pos.x >= nodePos.x && pos.x <= nodePos.x + nodeSize) {
			if (pos.y <= nodePos.y && pos.y >= nodePos.y - nodeSize) {
				if (pos.z >= nodePos.z && pos.z <= nodePos.z + nodeSize) {
					return true;
				}
			}
		}

		return false;
	}

	inline static bool isBoxInBox(const SFE::Math::Vec3& pos, const SFE::Math::Vec3& nodePos, const SFE::Math::Vec3& nodeSize) {
		if (pos.x >= nodePos.x - nodeSize.x && pos.x <= nodePos.x + nodeSize.x) {
			if (pos.y <= nodePos.y + nodeSize.y && pos.y >= nodePos.y - nodeSize.y) {
				if (pos.z >= nodePos.z - nodeSize.z && pos.z <= nodePos.z + nodeSize.z) {
					return true;
				}
			}
		}

		return false;
	}

	inline static Math::Vec3 getOctantPos(Octant boxOctant, float nodeSize) {
		return {
			 nodeSize * (boxOctant == RTN || boxOctant == RBN || boxOctant == RTF || boxOctant == RBF),
			-nodeSize * (boxOctant == LBN || boxOctant == RBN || boxOctant == LBF || boxOctant == RBF),
			 nodeSize * (boxOctant == LTN || boxOctant == RTN || boxOctant == LBN || boxOctant == RBN)
		};
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

	template<typename DataType>
	struct Object {
		friend bool operator<(const Object& lhs, const Object& rhs) {
			return lhs.data < rhs.data;
		}

		friend bool operator<=(const Object& lhs, const Object& rhs) {
			return !(rhs < lhs);
		}

		friend bool operator>(const Object& lhs, const Object& rhs) {
			return rhs < lhs;
		}

		friend bool operator>=(const Object& lhs, const Object& rhs) {
			return !(lhs < rhs);
		}

		friend bool operator==(const Object& lhs, const Object& rhs) {
			return lhs.data == rhs.data;
		}

		friend bool operator==(const Object& lhs, const DataType& rhs) {
			return lhs.data == rhs;
		}

		bool operator==(const Object& rhs) {
			return operator==(*this, rhs);
		}

		bool operator==(const DataType& rhs) {
			return operator==(*this, rhs);
		}

		bool operator!=(const Object& rhs) {
			return !(operator==(rhs));
		}

		bool operator!=(const DataType& rhs) {
			return !(operator==(rhs));
		}

		Object(const Math::Vec3& pos, const Math::Vec3& size, const DataType& data) : pos(pos), size(size), data(data) {}
		Object() = default;

		Math::Vec3 pos;
		Math::Vec3 size;
		DataType data;
	};

	template<typename DataType>
	struct OcTreeNode {

		using ObjectType = Object<DataType>;
		OcTreeNode() = default;

	public:
		OcTreeNode(const OcTreeNode& other)
			: children(other.children),
			  data(other.data) {}

		OcTreeNode(OcTreeNode&& other) noexcept
			: children(std::move(other.children)),
			  data(std::move(other.data)) {}

	public:
		OcTreeNode& operator=(const OcTreeNode& other) {
			if (this == &other)
				return *this;
			children = other.children;
			data = other.data;
			return *this;
		}

		OcTreeNode& operator=(OcTreeNode&& other) noexcept {
			if (this == &other)
				return *this;
			children = std::move(other.children);
			data = std::move(other.data);
			return *this;
		}
	public:

		OcTreeNode& operator[](size_t i) {
			std::shared_lock lock(mtx);
			return children[i];
		}

	public:
		inline void clear() {
			std::unique_lock lock(mtx);
			data.clear();
			data.shrink_to_fit();

			for (auto& child : children) {
				child.clear();
			}
			children.clear();
		}

		inline void erase(const DataType& data) {
			std::unique_lock lock(mtx);
			if (!this->data.empty()) {
				this->data.erase(std::remove_if(this->data.begin(), this->data.end(), [&data](ObjectType& _data) { return data == _data.data; }), this->data.end());
			}
			

			bool allEmpty = true;
			for (auto& child : children) {
				child.erase(data);
				allEmpty &= child.data.empty();
				allEmpty &= child.children.empty();
			}

			if (allEmpty) {
				children.clear();
			}
		}

		inline ObjectType* find(const DataType& _data) {
			std::shared_lock lock(mtx);
			for (auto& obj : data) {
				if (obj.data == _data) {
					return &obj;
				}
			}

			for (auto& child : children) {
				if (auto result = child.find(_data)) {
					return result;
				}
			}

			return nullptr;
		}

		inline bool contains(const DataType& _data) {
			return find(_data) != nullptr;
		}

	public:
		inline void forEach(const std::function<void(ObjectType&)>& func) {
			std::shared_lock lock(mtx);
			for (auto& obj : data) {
				func(obj);
			}

			for (auto& child : children) {
				child.forEach(func);
			}
		}

		inline void forEach(const std::function<void(const ObjectType&)>& func) const {
			std::shared_lock lock(mtx);
			for (auto& obj : data) {
				func(obj);
			}

			for (auto& child : children) {
				child.forEach(func);
			}
		}

		inline void forEachNode(const SFE::Math::Vec3& nodePos, float nodeSize, std::function<void(const SFE::Math::Vec3&, float, OcTreeNode&)> func) {
			std::shared_lock lock(mtx);
			if (children.empty()) {
				return;
			}

			nodeSize *= 0.5f;
			for (auto octant : traversal[0]) {
				func(nodePos + getOctantPos(static_cast<Octant>(octant), nodeSize), nodeSize, children[octant]);
			}
		}

		inline void forEachNode(const SFE::Math::Vec3& nodePos, float nodeSize, std::function<void(const ObjectType&)> func, std::function<bool(const SFE::Math::Vec3&, float, OcTreeNode&)> pred = nullptr) {
			if (pred) {
				if (!pred(nodePos, nodeSize, *this)) {
					return;
				}
			}

			for (const auto& object : data) {
				func(object);
			}

			forEachNode(nodePos, nodeSize, [func, pred](const SFE::Math::Vec3& pos, float size, OcTreeNode& node) {
				node.forEachNode(pos, size, func, pred);
			});
		}

	public:
		std::vector<ObjectType>& getData() { return data; }
		const std::vector<ObjectType>& getData() const { return data; }

	public:
		mutable std::shared_mutex mtx; //todo dirty
		std::vector<OcTreeNode> children;

	private:
		std::vector<ObjectType> data;
	};

	template<typename DataType, size_t Deep = 4, size_t Size = 256>
	struct OcTree : public ThreadSynchronizer {
	public:
		using NodeType = OcTreeNode<DataType>;
		using ObjectType = typename NodeType::ObjectType;
		
	public:
		OcTree(const Math::Vec3& octreePos) : mPos(octreePos) {}
		OcTree() : mPos{} {}

	public:
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
			auto octreeDiagonal = 3 * nodeSize * nodeSize;
			auto nodeDiagonal = size.x * size.x + size.y * size.y + size.z * size.z;
			auto diagonalsDistance = octreeDiagonal + nodeDiagonal;
			if (diagonalsDistance < SFE::Math::lengthSquared(nodePos - pos)) {
				return false;
			}



			const auto FBL = isPointInBox(pos - size, nodePos, nodeSize); //far bottom left
			if (FBL) {
				return true;
			}

			const auto NTR = isPointInBox(pos + size, nodePos, nodeSize); //near top right
			if (NTR) {
				return true;
			}

			const auto FBR = isPointInBox({ pos.x + size.x, pos.y - size.y, pos.z - size.z }, nodePos, nodeSize);
			if (FBR) {
				return true;
			}
			const auto FTL = isPointInBox({ pos.x - size.x, pos.y + size.y, pos.z - size.z }, nodePos, nodeSize);
			if (FTL) {
				return true;
			}
			const auto FTR = isPointInBox({ pos.x + size.x, pos.y + size.y, pos.z - size.z }, nodePos, nodeSize);
			if (FTR) {
				return true;
			}

			const auto NBR = isPointInBox({ pos.x + size.x, pos.y - size.y, pos.z + size.z }, nodePos, nodeSize);
			if (NBR) {
				return true;
			}
			const auto NTL = isPointInBox({ pos.x - size.x, pos.y + size.y, pos.z + size.z }, nodePos, nodeSize);
			if (NTL) {
				return true;
			}
			const auto NBL = isPointInBox({ pos.x - size.x, pos.y - size.y, pos.z + size.z }, nodePos, nodeSize);
			if (NBL) {
				return true;
			}

			// LTF - nodePos
			const auto aLBF = isBoxInBox(nodePos + Math::Vec3{0.f, -nodeSize, 0.f}, pos, size); //far bottom left
			if (aLBF) {
				return true;
			}

			const auto aRTN = isBoxInBox(nodePos + Math::Vec3{nodeSize, 0.f, nodeSize}, pos, size); //near top right
			if (aRTN) {
				return true;
			}

			const auto aRBF = isBoxInBox(nodePos + Math::Vec3{nodeSize, -nodeSize, 0.f}, pos, size);
			if (aRBF) {
				return true;
			}
			const auto aLTF = isBoxInBox(nodePos + Math::Vec3{0.f, 0.f, 0.f}, pos, size);
			if (aLTF) {
				return true;
			}
			const auto aRTF = isBoxInBox(nodePos + Math::Vec3{nodeSize, 0.f, 0.f}, pos, size);
			if (aRTF) {
				return true;
			}

			const auto aRBN = isBoxInBox(nodePos + Math::Vec3{nodeSize, -nodeSize, nodeSize}, pos, size);
			if (aRBN) {
				return true;
			}
			const auto aLTN = isBoxInBox(nodePos + Math::Vec3{0.f, 0.f, nodeSize}, pos, size);
			if (aLTN) {
				return true;
			}
			const auto aLBN = isBoxInBox(nodePos + Math::Vec3{0.f, 0.f, nodeSize}, pos, size);
			if (aLBN) {
				return true;
			}
			
			float left, right, top, btm, near, far;
			calculateAABBOctrees({ pos, (float)size.x, (float)size.y, (float)size.z }, nodeSize, left, right, top, btm, near, far);

			if (nodePos.x <= left && nodePos.x + nodeSize >= right) {
				if (nodePos.y <= top && nodePos.y - nodeSize >= btm) {
					if (nodePos.z <= near && nodePos.z + nodeSize >= far) {
						return true;
					}
				}
			}

			return false;
		}

		static inline bool isAABBInBox(const Math::Vec3& pos, const Math::Vec3& size, const Math::Vec3& nodePos, float nodeSize) {
			auto octreeDiagonal = 3 * nodeSize * nodeSize;
			auto nodeDiagonal = size.x * size.x + size.y * size.y + size.z * size.z;
			auto diagonalsDistance = octreeDiagonal + nodeDiagonal;
			if (diagonalsDistance < SFE::Math::lengthSquared(nodePos - pos)) {
				return false;
			}
			return isPointInBox(pos - size, nodePos, nodeSize) && isPointInBox(pos + size, nodePos, nodeSize);
		}

		static inline Math::Vec3 calculateCenter(const Math::Vec3& ltfPos, float size) {
			return ltfPos + Math::Vec3(size, -size, size) * 0.5f;
		}

		static inline bool isOnFrustum(const FrustumModule::Frustum& camFrustum, const SFE::Math::Vec3& ltf, float size) {
			return FrustumModule::SquareAABB::isOnFrustum(camFrustum, calculateCenter(ltf, size), size * 0.5f);
		}

	public:
		static inline void drawOctreeHelper(const NodeType& node, float nodeSize, const Math::Vec3& nodePos, bool drawObjAABB) {
			constexpr static Math::Mat4 rotate = {
				{1.f,0.f,0.f,0.f},
				{0.f,1.f,0.f,0.f},
				{0.f,0.f,1.f,0.f},
				{0.f,0.f,0.f,1.f}
			};


			
				constexpr static auto notEmptyColor = Math::Vec4(0.f, 0.5f, 0.f, 0.02f);

				Render::Utils::renderCubeMesh(
					Math::Vec3(0.f, 0.f, nodeSize),
					Math::Vec3(nodeSize, -nodeSize, 0.f),
					rotate, Math::Vec3(nodePos), notEmptyColor
				);

			if (node.getData().size()) {
				if (drawObjAABB) {
					for (auto& data : node.getData()) {
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

			int i = 0;
			for (const auto& child : node.children) {
				auto tmpPos = nodePos + getOctantPos(static_cast<Octant>(i), nodeSize * 0.5f);
				drawOctreeHelper(child, nodeSize * 0.5f, tmpPos, drawObjAABB);
				i++;
			}
		}

		inline void drawOctree(bool drawObjAABB) {
			drawOctreeHelper(root, mSize, mPos, drawObjAABB);
		}

	public:
		inline void forEachObjectInFrustum(const SFE::FrustumModule::Frustum& frustum, std::function<void(const ObjectType&)> func) {
			root.forEachNode(mPos, mSize, func, [&frustum](const SFE::Math::Vec3& pos, float size, NodeType& node) {
				return isOnFrustum(frustum, pos, size);
			});
		}

		inline void forEachObject(std::function<void(const ObjectType&)> func, std::function<bool(const SFE::Math::Vec3&, float, NodeType&)> pred) {
			root.forEachNode(mPos, mSize, func, pred);
		}

		inline void forEach(const std::function<void(ObjectType&)>& func) {
			root.forEach(func);
		}

		inline void forEach(const std::function<void(const ObjectType&)>& func) const {
			root.forEach(func);
		}

	public:
		bool contains(const DataType& data) {
			return root.contains(data);
		}

		ObjectType* find(const DataType& data) {
			return root.find(data);
		}

		void clear() {
			root.clear();
		}

		void erase(const DataType& data) {
			root.erase(data);
		}

		bool insert(const SFE::Math::Vec3& globalPos, const SFE::Math::Vec3& size, const DataType& data) {
			if (!isAABBInBoxAny(globalPos, size, mPos, mSize)) {
				return false;
			}

			if (!insertRecursive(root, mPos, globalPos, size, 0, mSize, data)) {
				std::unique_lock lock(root.mtx);
				root.getData().emplace_back(globalPos, size, data);
			}

			return true;
		}

		bool insertRecursive(NodeType& node, const SFE::Math::Vec3& nodePos, const SFE::Math::Vec3& localPos, const SFE::Math::Vec3& size, size_t step, float nodeSize, const DataType& data) {
			if (!isAABBInBox(localPos, size, nodePos, nodeSize)) {
				return false;
			}

			nodeSize *= 0.5f;

			bool inserted = false;
			for (auto octant : traversal[0]) {
				auto tmpPos = nodePos + getOctantPos(static_cast<Octant>(octant), nodeSize);

				if (isAABBInBox(localPos, size, tmpPos, nodeSize)) {
					if (step + 1 < Deep) {
						node.children.resize(8);
						inserted |= insertRecursive(node.children[octant], tmpPos, localPos, size, step + 1, nodeSize, data);
					}
				}
			}

			if (!inserted) {
				inserted = true;
				std::unique_lock lock(node.mtx);
				node.getData().emplace_back(localPos, size, data);
			}
			return inserted;
		}

	public:
		std::vector<std::pair<SFE::Math::Vec3, ObjectType>> findCollisions(const SFE::Math::Vec3& src, const Math::Vec3& dir, const std::function<bool(const ObjectType& data)>& pred = nullptr) {
			std::vector<std::pair<SFE::Math::Vec3, ObjectType>> result;
			findCollisions(root, result, src, dir, mPos, mSize, pred);

			std::sort(result.begin(), result.end(), [src](const std::pair<SFE::Math::Vec3, ObjectType>& a, const std::pair<SFE::Math::Vec3, ObjectType>& b) {
				return SFE::Math::lengthSquared(src - a.first) < SFE::Math::lengthSquared(src - b.first);
			});

			return result;
		}

		bool findCollisions(const NodeType& node, std::vector<std::pair<SFE::Math::Vec3, ObjectType>>& collisions, const SFE::Math::Vec3& src, const SFE::Math::Vec3& dir, SFE::Math::Vec3 nodePos, float nodeSize, const std::function<bool(const ObjectType& data)>& pred) {
			const auto octant = getSourceCardinalDirection(nodePos, nodeSize, src);

			nodeSize *= 0.5f;

			const auto collisionRes = PhysicsEngine::Physics::checkCollision(
				{{ nodePos.x + nodeSize, nodePos.y - nodeSize, nodePos.z + nodeSize }, {nodeSize}},
				src,
				dir
			);

			if (!collisionRes.second) {
				return false;
			}

			std::shared_lock lock(node.mtx);
			bool collisionFound = false; 
			for (const auto& nodeData : node.getData()) {
				auto cube = PhysicsEngine::Cube{ nodeData.pos, nodeData.size };
				auto collision = PhysicsEngine::Physics::checkCollision(cube, src, dir, true);

				if (collision.second && isPointInBox(collision.first, mPos, mSize) && (pred ? pred(nodeData) : true)) {//todo why mPos and mSize?!
					collisions.emplace_back(collision.first, nodeData);
					collisionFound = true;
				}
			}

			if (!node.children.empty()) {
				for (auto idx : traversal[octant]) {
					const auto tmpPos = nodePos + getOctantPos(static_cast<Octant>(idx), nodeSize);

					if (findCollisions(node.children[idx], collisions, src, dir, tmpPos, nodeSize, pred)) {
						return true;
					}
				}
			}
			

			return collisionFound;
		}

		inline bool isOnFrustum(const FrustumModule::Frustum& camFrustum) const {
			return isOnFrustum(camFrustum, mPos, mSize);
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

		float mSize = static_cast<float>(Size);
		Math::Vec3 mPos; //LTF left top far point //todo instead LTF use minimum

		NodeType root;
	};
}
