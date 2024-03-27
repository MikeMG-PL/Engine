#pragma once

#include <glm/glm.hpp>

struct ConstantBuffer
{

};

struct ConstantBufferPerObject
{
    glm::mat4 projection_view;
    glm::mat4 model;
    glm::mat4 projection;
};

struct ConstantBufferSkybox : public ConstantBuffer
{
    glm::mat4 PV_no_translation;
};
