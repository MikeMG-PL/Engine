#include "Editor.h"

#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include "Entity.h"
#include "Camera.h"
#include "SceneSerializer.h"
#include "Engine.h"
#include "Input.h"
#include "RendererDX11.h"

namespace Editor
{
Editor::Editor(std::shared_ptr<Scene> const& scene) : m_open_scene(scene)
{
    set_style();

    m_debug_window.flags |= ImGuiWindowFlags_MenuBar;
    m_last_second = glfwGetTime();
}

void Editor::draw()
{
    if (!m_rendering_to_editor)
        return;

    draw_debug_window();
    draw_scene_hierarchy();
    draw_game();
    draw_inspector();
}

void Editor::set_scene(std::shared_ptr<Scene> const& scene)
{
    m_open_scene = scene;
}

void Editor::draw_debug_window()
{
    m_current_time = glfwGetTime();
    m_frame_count += 1;

    if (m_current_time - m_last_second >= 1.0)
    {
        m_average_ms_per_frame = 1000.0 / static_cast<double>(m_frame_count);
        m_frame_count = 0;
        m_last_second = glfwGetTime();
    }

    ImGui::Begin("Debug", &m_debug_window.open, m_debug_window.flags);
    ImGui::Checkbox("Polygon mode", &m_polygon_mode_active);
    ImGui::Text("Application average %.3f ms/frame", m_average_ms_per_frame);
    draw_scene_save();
    ImGui::End();

    Renderer::get_instance()->wireframe_mode_active = m_polygon_mode_active;
}

void Editor::draw_game()
{
    ImGui::Begin("Scene", &m_game_window.open, m_game_window.flags);

    auto vec2 = ImGui::GetContentRegionAvail();
    m_game_size = { vec2.x, vec2.y };

    vec2 = ImGui::GetWindowPos();
    m_game_position = { vec2.x, vec2.y };

    if (Renderer::renderer_api == Renderer::RendererApi::DirectX11)
    {
        ImGui::Image(RendererDX11::get_instance_dx11()->get_render_texture_view(), ImVec2(m_game_size.x, m_game_size.y));
    }

    if (m_selected_entity.expired())
    {
        ImGui::End();
        return;
    }

    auto const camera = Camera::get_main_camera();
    auto const entity = m_selected_entity.lock();

    ImGuizmo::SetDrawlist();

    ImGuizmo::SetRect(m_game_position.x, m_game_position.y, m_game_size.x, m_game_size.y);

    bool was_transform_changed = false;
    glm::mat4 global_model = entity->transform->get_model_matrix();
    switch (m_operation_type)
    {
    case GuizmoOperationType::Translate:
        was_transform_changed = ImGuizmo::Manipulate(glm::value_ptr(camera->get_view_matrix()), glm::value_ptr(camera->get_projection()), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(global_model), nullptr, nullptr);
        break;
    case GuizmoOperationType::Scale:
        was_transform_changed = ImGuizmo::Manipulate(glm::value_ptr(camera->get_view_matrix()), glm::value_ptr(camera->get_projection()), ImGuizmo::OPERATION::SCALE, ImGuizmo::MODE::LOCAL, glm::value_ptr(global_model), nullptr, nullptr);
        break;
    case GuizmoOperationType::Rotate:
        was_transform_changed = ImGuizmo::Manipulate(glm::value_ptr(camera->get_view_matrix()), glm::value_ptr(camera->get_projection()), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(global_model), nullptr, nullptr);
        break;
    case GuizmoOperationType::None:
    default:
        break;
    }

    if (was_transform_changed)
    {
        glm::mat4 local = global_model;

        auto const parent = entity->transform->parent.lock();
        if (parent != nullptr)
        {
            local = glm::inverse(parent->get_model_matrix()) * local;
        }

        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local), glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale));
        if (m_operation_type == GuizmoOperationType::Translate)
        {
            entity->transform->set_local_position(position);
        }
        else if (m_operation_type == GuizmoOperationType::Rotate)
        {
            entity->transform->set_euler_angles(rotation);
        }
        else if (m_operation_type == GuizmoOperationType::Scale)
        {
            entity->transform->set_local_scale(scale);
        }
    }

    ImGui::End();
}

void Editor::draw_scene_hierarchy()
{
    ImGui::Begin("Hierarchy", &m_hierarchy_window.open, m_hierarchy_window.flags);

    // Draw every entity without a parent, and draw its children recursively
    for (auto const& entity : m_open_scene->entities)
    {
        if (!entity->transform->parent.expired())
            continue;

        draw_entity_recursively(entity->transform);
    }
    ImGui::End();
}

void Editor::draw_entity_recursively(std::shared_ptr<Transform> const& transform)
{
    auto const entity = transform->entity.lock();
    ImGuiTreeNodeFlags const node_flags = (!m_selected_entity.expired() && m_selected_entity.lock()->hashed_guid == entity->hashed_guid ? ImGuiTreeNodeFlags_Selected : 0) | (
        transform->children.empty() ? ImGuiTreeNodeFlags_Leaf : 0) | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;

    if (!ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity->hashed_guid)), node_flags, "%s", entity->name.c_str()))
    {
        if (ImGui::IsItemClicked())
            m_selected_entity = entity;

        return;
    }

    if (ImGui::IsItemClicked())
        m_selected_entity = entity;

    for (auto const& child : transform->children)
    {
        draw_entity_recursively(child);
    }

    ImGui::TreePop();
}

void Editor::draw_inspector()
{
    ImGui::Begin("Inspector", &m_inspector_window.open, m_inspector_window.flags);

    if (m_selected_entity.expired())
    {
        ImGui::End();
        return;
    }

    auto const camera = Camera::get_main_camera();
    auto const entity = m_selected_entity.lock();

    ImGui::Text("Transform");
    ImGui::Spacing();

    glm::vec3 position = entity->transform->get_local_position();
    ImGui::InputFloat3("Position", glm::value_ptr(position));
    entity->transform->set_local_position(position);

    glm::vec3 rotation = entity->transform->get_euler_angles();
    ImGui::InputFloat3("Rotation", glm::value_ptr(rotation));
    entity->transform->set_euler_angles(rotation);

    glm::vec3 scale = entity->transform->get_local_scale();
    ImGui::InputFloat3("Scale", glm::value_ptr(scale));
    entity->transform->set_local_scale(scale);

    for (auto const& component : entity->components)
    {
        ImGui::Text(component->get_name().c_str());
        ImGui::Spacing();

        bool enabled = component->enabled();
        ImGui::Checkbox("Enabled", &enabled);
        component->set_enabled(enabled);

        component->draw_editor();

        ImGui::Spacing();
    }

    ImGui::End();
}

void Editor::draw_scene_save() const
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save scene"))
            {
                save_scene();
            }

            if (ImGui::MenuItem("Load scene"))
            {
                load_scene();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void Editor::save_scene() const
{
    auto const scene_serializer = std::make_shared<SceneSerializer>(m_open_scene);
    scene_serializer->serialize("./res/scenes/scene.txt");
}

bool Editor::load_scene() const
{
    auto const scene_serializer = std::make_shared<SceneSerializer>(m_open_scene);

    return scene_serializer->deserialize("./res/scenes/scene.txt");
}

void Editor::set_style() const
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}

void Editor::handle_input()
{
    auto const input = Input::input;

    if (input->get_key_down(GLFW_KEY_F1))
    {
        switch_rendering_to_editor();
    }

    if (input->get_key_down(GLFW_KEY_W))
    {
        m_operation_type = GuizmoOperationType::Translate;
    }

    if (input->get_key_down(GLFW_KEY_R))
    {
        m_operation_type = GuizmoOperationType::Scale;
    }

    if (input->get_key_down(GLFW_KEY_E))
    {
        m_operation_type = GuizmoOperationType::Rotate;
    }

    if (input->get_key_down(GLFW_KEY_G))
    {
        m_operation_type = GuizmoOperationType::None;
    }

    if (input->get_key_down(GLFW_KEY_F5))
    {
        Renderer::get_instance()->reload_shaders();
    }
}

void Editor::set_docking_space() const
{
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void Editor::switch_rendering_to_editor()
{
    Renderer::get_instance()->switch_rendering_to_texture();
    m_rendering_to_editor = !m_rendering_to_editor;
}

}
