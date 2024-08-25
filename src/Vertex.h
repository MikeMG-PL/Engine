#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texture_coordinates;
    glm::ivec4 skin_indices = {-1, -1, -1, -1};
    glm::vec4 skin_weights = {0.0f, 0.0f, 0.0f, 0.0f};
};
