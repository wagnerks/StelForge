#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"
#include "assetsModule/Asset.h"
#include "mathModule/Quaternion.h"
#include "nodeModule/TreeNode.h"


namespace AssetsModule {
	class Animation;

	struct _Bone {
		std::string name;

		SFE::Math::Mat4 offset;

		SFE::Math::Mat4 transform;
		SFE::Math::Vec3 pos;
		SFE::Math::Vec3 scale;
		SFE::Math::Quaternion<float> rotation;

		uint32_t parentBoneIdx = std::numeric_limits<uint32_t>::max();
		std::vector<uint32_t> childrenBones;
	};

	class Armature {
	public:
		std::string name;
		std::vector<_Bone>& getBones() { return mBones; }
	private:
		std::vector<_Bone> mBones;
	};

	struct BoneInfo
	{
		/*id is index in finalBoneMatrices*/
		int id;

		/*offset matrix transforms vertex from model space to bone space*/
		SFE::Math::Mat4 offset;

	};

	struct ModelObj {
		std::vector<MeshHandle> mMeshHandles;
	};

	struct MeshNode : SFE::NodeModule::TreeNode<MeshNode> {
		MeshNode() = default;

		MeshNode(MeshNode&& other) noexcept
			: TreeNode<MeshNode>(std::move(other)),
			mMeshes(std::move(other.mMeshes))
		{}

		MeshNode& operator=(MeshNode&& other) noexcept {
			TreeNode<MeshNode>::operator =(std::move(other));
			if (this == &other) {
				return *this;
			}

			mMeshes = std::move(other.mMeshes);
			return *this;
		}

		Armature armature;

		std::vector<std::vector<Mesh>> mMeshes;
	};

	class Model : public Asset {
	public:

		Model(MeshNode model, std::string_view modelPath);

		std::map<std::string, BoneInfo> mBones;

		const std::map<std::string, BoneInfo>& getBoneInfoMap() { return mBones; }

		std::vector<ModelObj>* getAllLODs();

		ModelObj toModelObj(int lod);

		std::string_view getModelPath();

		void normalizeModel(bool smooth = true);

		void bindMeshes();

		bool normalized = false;
		int mLODs = 0;

		Animation* anim;

		void calculateLODs();
		void toModelObjHelper(MeshNode* root, int lod, ModelObj& res);
		std::vector<ModelObj> lods;
		MeshNode mMeshTree;
		std::string mModelPath = "";
	};
}
