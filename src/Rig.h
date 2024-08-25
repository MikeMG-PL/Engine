#pragma once
#include "AK/Math.h"

#include <string>
#include <vector>

struct Rig
{
    std::vector<std::string> bone_names = {};
    std::vector<i32> parents = {};
    std::vector<AK::xform> ref_pose = {};
    u32 num_bones = 0;
};
