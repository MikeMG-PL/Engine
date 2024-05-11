#pragma once

#include <string>
#include <vector>
#include <glm/vec3.hpp>

enum class DebugType
{
    Log,
    Warning,
    Error
};

struct DebugMessage
{
    DebugType type = DebugType::Log;
    std::string text = {};
};

class Debug
{
public:
    static void log(std::string const& message, DebugType type = DebugType::Log);
    static void clear();
    static void draw_debug_sphere(glm::vec3 const position, float const radius = 0.1f, float const time = 0.0f);
    static void draw_debug_box(glm::vec3 const position, glm::vec3 const euler_angles = {0.0f, 0.0f, 0.0f},
                               glm::vec3 const extents = {0.25f, 0.25f, 0.25f}, float const time = 0.0f);

    inline static std::vector<DebugMessage> debug_messages = {};
};
