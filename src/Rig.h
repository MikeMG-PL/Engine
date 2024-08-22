#pragma once
#include "AK/Types.h"

#include <string>
#include <vector>

namespace AK
{
struct xform;
}

struct Rig
{
    std::vector<std::string> bone_names = {};
    std::vector<i32> parents = {};
    std::vector<AK::xform> ref_pose = {};
    u32 num_bones = 0;
};
