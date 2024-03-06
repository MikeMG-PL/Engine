#pragma once

#include <imgui.h>
#include <glad/glad.h> // Initialize with gladLoadGL()

#include <GLFW/glfw3.h> // Include glfw3.h after our OpenGL definitions

#include <miniaudio.h>

#include <memory>

#include "Window.h"
#include "AK/Types.h"

class Engine
{
public:
    static i32 initialize();
    static void create_game();
    static void run();
    static void clean_up();

    inline static bool enable_vsync = false;
    inline static bool enable_mouse_capture = true;

    inline static i32 screen_width = 1600;
    inline static i32 screen_height = 900;

    inline static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    inline static ma_engine audio_engine;

    inline static std::shared_ptr<Window> window;
private:
    static i32 initialize_thirdparty();

    static i32 setup_glfw();
    static std::shared_ptr<Window> create_window();
    static void glfw_error_callback(i32 const error, char const* description);

    static i32 setup_glad();

    static void setup_imgui(GLFWwindow* glfw_window);

    static i32 setup_miniaudio();
};
