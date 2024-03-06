#include "Rig.h"

#include <vector>

#include "AnimXForm.h"
#include "aiHelpers.h"
#include "glm/gtx/quaternion.hpp"

void Rig::LocalToModel(std::vector<xform>& modelPose, const std::vector<xform>& localPose)
{
	numBones = localPose.size();
	modelPose.resize(numBones);

	for (int i = 0; i < numBones; i++)
	{
		const int idParent = parents[i];
		if (idParent >= 0)
		{
			// There might be something wrong here: ORDER of multiplication, parent indices or bones
			// might not be sorted by parent
			modelPose[i] = localPose[i] * modelPose[idParent];
		}
		else
			modelPose[i] = localPose[i];
	}

	// Remember to: inv bind pose * pose in model space = skinning matrices
}

// DEPRECATED, data from the function got overriden somehow, moved to RiggedModel's Draw().
void Rig::MultiplyInverseBindPoseByModelPose(const std::vector<xform>& modelPose)
{
	//skinnedPose.resize(numBones);

	// for(int i = 0; i < numBones; i++)
	// {
	// 	xform inverseBindPoseXForm, skinnedXForm;
	// 	glm::vec3 scale;
	// 	glm::quat rotation;
	// 	glm::vec3 translation;
	// 	glm::vec3 skew;
	// 	glm::vec4 perspective;
	// 	glm::decompose(inverseBindPose[i], scale, rotation, translation, skew, perspective);
	//
	// 	inverseBindPoseXForm.position = translation;
	// 	inverseBindPoseXForm.rotation = rotation;
	//
	// 	skinnedXForm = inverseBindPoseXForm * modelPose[i];
	//
	// 	glm::mat4 skinningMatrix = glm::mat4(1.0f);
	// 	skinningMatrix = glm::translate(skinningMatrix, skinnedXForm.position);
	// 	skinningMatrix = skinningMatrix * glm::toMat4(skinnedXForm.rotation);
	//
	// 	skinnedPose[i] = skinningMatrix;
	// }
}
