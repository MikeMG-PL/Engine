#include "Engine.h"

#include <utility>

#include <imgui_impl/imgui_impl_glfw.h>
#include <imgui_impl/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <miniaudio.h>

#include "Editor.h"
#include "Globals.h"
#include "Input.h"
#include "MainScene.h"
#include "Renderer.h"
#include "RendererDX11.h"
#include "RendererGL.h"
#include "Window.h"
#include "Game/Game.h"
#include "imgui_impl/imgui_impl_dx11.h"

i32 Engine::initialize()
{
    if (auto const result = initialize_thirdparty_before_renderer(); result != 0)
        return result;

    switch (Renderer::renderer_api)
    {
    case Renderer::RendererApi::OpenGL:
        static_cast<void>(RendererGL::create());
        break;
    case Renderer::RendererApi::DirectX11:
        static_cast<void>(RendererDX11::create());
        break;
    default:
        std::unreachable();
    }

    Renderer::get_instance()->set_vsync(enable_vsync);

    if (auto const result = initialize_thirdparty_after_renderer(); result != 0)
        return result;

    if (Renderer::renderer_api == Renderer::RendererApi::OpenGL)
        InternalMeshData::initialize();
    else if (Renderer::renderer_api == Renderer::RendererApi::DirectX11)
        std::cout << "TODO: Initialize internal mesh data in DX11 API." << "\n";

    return 0;
}

void Engine::create_game()
{
    auto const main_scene = std::make_shared<Scene>();
    MainScene::set_instance(main_scene);

    // Custom initialization code
    auto const game = std::make_shared<Game>(window);
    game->initialize();
}

void Engine::run()
{
    Renderer::get_instance()->initialize();

    bool debug_open = true;
    bool polygon_mode = false;

    i32 nb_frames = 0;
    double frame_per_second = 0.0;
    double last_time = glfwGetTime();
    double last_frame = 0.0; // Time of last frame

    Editor::Editor editor(MainScene::get_instance());

    // Main loop
    while (!glfwWindowShouldClose(window->get_glfw_window()))
    {
        double const current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        nb_frames++;
        if (current_frame - last_time >= 1.0)
        {
            frame_per_second = 1000.0 / static_cast<double>(nb_frames);
            nb_frames = 0;
            last_time = glfwGetTime();
        }

        glfwPollEvents();
        Input::input->update_keys();

        // Start the Dear ImGui frame
        switch (Renderer::renderer_api)
        {
        case Renderer::RendererApi::OpenGL:
            ImGui_ImplOpenGL3_NewFrame();
            break;
        case Renderer::RendererApi::DirectX11:
            ImGui_ImplDX11_NewFrame();
            break;
        default:
            std::unreachable();
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags window_flags = 0;
        window_flags |= ImGuiWindowFlags_MenuBar;

        ImGui::Begin("Debug", &debug_open, window_flags);
        ImGui::Checkbox("Polygon mode", &polygon_mode);
        ImGui::Text("Application average %.3f ms/frame", frame_per_second);

        editor.draw_scene_save();
        ImGui::End();

        editor.draw_scene_hierarchy();
        editor.draw_inspector();


        Renderer::get_instance()->wireframe_mode_active = polygon_mode;

        Renderer::get_instance()->begin_frame();

        MainScene::get_instance()->run_frame();

        Renderer::get_instance()->render();

        Renderer::get_instance()->end_frame();


        ImGui::Render();

        switch (Renderer::renderer_api)
        {
        case Renderer::RendererApi::OpenGL:
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwMakeContextCurrent(window->get_glfw_window());
            glfwSwapBuffers(window->get_glfw_window());
            break;
        case Renderer::RendererApi::DirectX11:
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            break;
        default:
            std::unreachable();
        }

        Renderer::get_instance()->present();
    }
}

void Engine::clean_up()
{
    switch (Renderer::renderer_api)
    {
    case Renderer::RendererApi::OpenGL:
        ImGui_ImplOpenGL3_Shutdown();
        break;
    case Renderer::RendererApi::DirectX11:
        ImGui_ImplDX11_Shutdown();
        break;
    default:
        std::unreachable();
    }

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window->get_glfw_window());
    glfwTerminate();
}

std::shared_ptr<Window> Engine::create_window()
{
    // Create window with graphics context
    auto new_window = std::make_shared<Window>(
        Renderer::renderer_api,
        Renderer::screen_width,
        Renderer::screen_height,
        4,
        enable_vsync,
        enable_mouse_capture
    );

    return new_window;
}

i32 Engine::initialize_thirdparty_before_renderer()
{
    if (setup_glfw() != 0)
        return 1;

    window = create_window();

    if (window == nullptr)
        return 2;

    // NOTE: Creating input callbacks needs to happen BEFORE setting up imgui (for some unknown reason)
    auto const input_system = std::make_shared<Input>(window);
    Input::set_input(input_system);

    // TODO: Move this to RendererGL?
    if (Renderer::renderer_api == Renderer::RendererApi::OpenGL)
    {
        if (setup_glad() != 0)
            return 3;
    }

    srand(static_cast<u32>(glfwGetTime()));

    if (setup_miniaudio() != 0)
        return 4;

    return 0;
}

i32 Engine::initialize_thirdparty_after_renderer()
{
    setup_imgui(window->get_glfw_window());

    return 0;
}

i32 Engine::setup_glfw()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    return 0;
}

void Engine::glfw_error_callback(i32 const error, char const* description)
{
    (void)fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

i32 Engine::setup_glad()
{
    // Initialize OpenGL loader
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        spdlog::error("Failed to initialize OpenGL loader!");
        return 1;
    }
    spdlog::info("Successfully initialized OpenGL loader!");

    return 0;
}

void Engine::setup_imgui(GLFWwindow* glfw_window)
{
    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // GL 4.3 + GLSL 430
    auto const glsl_version = "#version 430";

    switch (Renderer::renderer_api)
    {
    case Renderer::RendererApi::OpenGL:
        ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
        break;
    case Renderer::RendererApi::DirectX11:
    {
        std::shared_ptr<RendererDX11> const renderer_dx = std::dynamic_pointer_cast<RendererDX11>(Renderer::get_instance());
        ImGui_ImplGlfw_InitForOther(glfw_window, true);
        ImGui_ImplDX11_Init(renderer_dx->get_device(), renderer_dx->get_device_context());
        break;
    }
    default:
        std::unreachable();
    }

    // Setup style
    ImGui::StyleColorsDark();
}

i32 Engine::setup_miniaudio()
{
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;
    config.sampleRate = 48000;
    config.listenerCount = 1;

    if (ma_engine_init(&config, &audio_engine) != MA_SUCCESS)
        return -1;

    if (ma_engine_start(&audio_engine) != MA_SUCCESS)
        return -2;

    return 0;
}
