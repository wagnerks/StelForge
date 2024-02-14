#include "Bone.h"

namespace AssetsModule {
	Bone::Bone(const std::string& name, int ID, const aiNodeAnim* channel) :
		mName(name),
		mID(ID),
		mLocalTransform(1.0f) {

		mPositions.reserve(channel->mNumPositionKeys);
		for (int positionIndex = 0; positionIndex < channel->mNumPositionKeys; ++positionIndex) {
			aiVector3D& aiPosition = channel->mPositionKeys[positionIndex].mValue;
			KeyPosition data;
			data.position = { aiPosition.x, aiPosition.y, aiPosition.z };
			data.timeStamp = channel->mPositionKeys[positionIndex].mTime;
			mPositions.push_back(data);
		}

		mRotations.reserve(channel->mNumRotationKeys);
		for (int rotationIndex = 0; rotationIndex < channel->mNumRotationKeys; ++rotationIndex) {
			aiQuaternion& aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
			KeyRotation data;
			data.orientation = { aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z };
			data.timeStamp = channel->mRotationKeys[rotationIndex].mTime;
			mRotations.push_back(data);
		}

		mScales.reserve(channel->mNumScalingKeys);
		for (int keyIndex = 0; keyIndex < channel->mNumScalingKeys; ++keyIndex) {
			aiVector3D& scale = channel->mScalingKeys[keyIndex].mValue;
			KeyScale data;
			data.scale = { scale.x, scale.y, scale.z };
			data.timeStamp = channel->mScalingKeys[keyIndex].mTime;
			mScales.push_back(data);
		}
	}

	void Bone::update(float animationTime) {
		mLocalTransform = interpolatePosition(animationTime) * interpolateRotation(animationTime) * interpolateScaling(animationTime);
	}

	int Bone::getPositionIndex(float animationTime) const {
		int index = 0;
		for (; index < mPositions.size() - 1; ++index) {
			if (animationTime < mPositions[index + 1].timeStamp) {
				return index;
			}
		}
		return index-1;
	}

	int Bone::getRotationIndex(float animationTime) const {
		int index = 0;
		for (; index < mRotations.size() - 1; ++index) {
			if (animationTime < mRotations[index + 1].timeStamp) {
				return index;
			}
		}
		return index-1;
	}

	int Bone::getScaleIndex(float animationTime) const {
		int index = 0;
		for (;index < mScales.size() - 1; ++index) {
			if (animationTime < mScales[index + 1].timeStamp) {
				return index;
			}
		}
		return index-1;
	}

	float Bone::calcScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) {
		const float midWayLength = animationTime - lastTimeStamp;
		const float framesDiff = nextTimeStamp - lastTimeStamp;
		return midWayLength / framesDiff;
	}

	SFE::Math::Mat4 Bone::interpolatePosition(float animationTime) const {
		if (mPositions.size() == 1) {
			return SFE::Math::translate(SFE::Math::Mat4(1.0f), mPositions[0].position);
		}

		const auto curIndex = getPositionIndex(animationTime);

		const auto& current = mPositions[getPositionIndex(animationTime)];
		const auto& next = mPositions[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, animationTime);

		return SFE::Math::translate(SFE::Math::Mat4(1.0f), mix(current.position, next.position, scaleFactor));
	}

	SFE::Math::Mat4 Bone::interpolateRotation(float animationTime) const {
		if (mRotations.size() == 1) {
			return normalize(mRotations[0].orientation).toMat4();
		}

		const auto curIndex = getPositionIndex(animationTime);

		const auto& current = mRotations[getPositionIndex(animationTime)];
		const auto& next = mRotations[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, animationTime);

		return normalize(slerp(current.orientation, next.orientation, scaleFactor)).toMat4();
	}

	SFE::Math::Mat4 Bone::interpolateScaling(float animationTime) const {
		if (mScales.size() == 1) {
			return scale(SFE::Math::Mat4(1.0f), mScales[0].scale);
		}

		const auto curIndex = getPositionIndex(animationTime);

		const auto& current = mScales[getPositionIndex(animationTime)];
		const auto& next = mScales[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, animationTime);

		return scale(SFE::Math::Mat4(1.0f), mix(current.scale, next.scale, scaleFactor));
	}
}

