#include "Renderer.h"

#include <array>
#include <format>
#include <iostream>
#include <glad/glad.h>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include "AK/AK.h"
#include "Camera.h"
#include "Entity.h"
#include "ShaderFactory.h"
#include "Skybox.h"

void Renderer::initialize()
{
    initialize_global_renderer_settings();

    frustum_culling_shader = ShaderFactory::create("./res/shaders/frustum_culling.glsl");

    // TODO: GPU instancing on one material currently supports only the first mesh that was bound to the material.
    size_t max_size = 0;
    for (auto const& material : instanced_materials)
    {
        material->model_matrices.reserve(material->drawables.size());
        material->bounding_boxes = std::vector<BoundingBoxShader>(material->drawables.size());

        if (max_size < material->drawables.size())
            max_size = material->drawables.size();
    }

    initialize_buffers(max_size);
}

void Renderer::register_shader(std::shared_ptr<Shader> const& shader)
{
    shaders.emplace_back(shader);
}

void Renderer::unregister_shader(std::shared_ptr<Shader> const& shader)
{
    AK::swap_and_erase(shaders, shader);
}

void Renderer::register_drawable(std::shared_ptr<Drawable> const& drawable)
{
    drawable->material->drawables.emplace_back(drawable);
}

void Renderer::register_material(std::shared_ptr<Material> const& material)
{
    if (material->is_gpu_instanced)
        instanced_materials.emplace_back(material);

    if (material->has_custom_render_order())
        custom_render_order_materials.insert( { material->get_render_order(), material });
}

void Renderer::unregister_material(std::shared_ptr<Material> const& material)
{
    if (material->is_gpu_instanced)
        AK::swap_and_erase(instanced_materials, material);

    // FIXME: Not sure if find works
    if (material->has_custom_render_order())
        custom_render_order_materials.erase(custom_render_order_materials.find( { material->get_render_order(), material }));
}

void Renderer::register_light(std::shared_ptr<Light> const& light)
{
    if (auto const potential_point_light = std::dynamic_pointer_cast<PointLight>(light))
    {
        point_lights.emplace_back(potential_point_light);
    }
    else if (auto const potential_spot_light = std::dynamic_pointer_cast<SpotLight>(light))
    {
        spot_lights.emplace_back(potential_spot_light);
    }
    else if (auto const potential_directional_light = std::dynamic_pointer_cast<DirectionalLight>(light))
    {
        // Don't assert here
        assert(directional_light == nullptr);

        directional_light = potential_directional_light;
    }

    lights.emplace_back(light);
}

void Renderer::unregister_light(std::shared_ptr<Light> const& light)
{
    AK::swap_and_erase(lights, light);
}

void Renderer::render() const
{
    if (Camera::get_main_camera() == nullptr)
        return;

    // Premultiply projection and view matrices
    glm::mat4 const projection_view = Camera::get_main_camera()->get_projection() * Camera::get_main_camera()->get_view_matrix();
    glm::mat4 const projection_view_no_translation = Camera::get_main_camera()->get_projection() * glm::mat4(glm::mat3(Camera::get_main_camera()->get_view_matrix()));

    for (auto const& shader : shaders)
    {
        shader->use();

        set_shader_uniforms(shader, projection_view, projection_view_no_translation);

        for (auto const& material : shader->materials)
        {
            if (material->has_custom_render_order())
                continue;

            if (material->is_gpu_instanced)
                draw_instanced(material, projection_view, projection_view_no_translation);
            else
                draw(material, projection_view);
        }
    }

    for (auto const& [render_order, material] : custom_render_order_materials)
    {
        material->shader->use();

        set_shader_uniforms(material->shader, projection_view, projection_view_no_translation);

        if (material->is_gpu_instanced)
            draw_instanced(material, projection_view, projection_view_no_translation);
        else
            draw(material, projection_view);
    }
}

void Renderer::draw(std::shared_ptr<Material> const& material, glm::mat4 const& projection_view) const
{
    material->shader->set_vec3("material.color", glm::vec3(material->color.x, material->color.y, material->color.z));
    material->shader->set_float("material.specular", material->specular);
    material->shader->set_float("material.shininess", material->shininess);

    material->shader->set_float("radiusMultiplier", material->radius_multiplier);
    material->shader->set_int("sector_count", material->sector_count);
    material->shader->set_int("stack_count", material->stack_count);

    for (auto const& drawable : material->drawables)
    {
        if (material->needs_view_model)
            material->shader->set_mat4("VM", Camera::get_main_camera()->get_view_matrix() * drawable->entity->transform->get_model_matrix());

        if (material->needs_skybox)
            Skybox::bind_skybox();

        material->shader->set_mat4("PVM", projection_view * drawable->entity->transform->get_model_matrix());
        material->shader->set_mat4("model", drawable->entity->transform->get_model_matrix());

        drawable->draw();
    }
}

void Renderer::draw_instanced(std::shared_ptr<Material> const& material, glm::mat4 const& projection_view, glm::mat4 const& projection_view_no_translation) const
{
    if (material->drawables.empty())
        return;

    auto const first_drawable = material->first_drawable;
    auto const shader = material->shader;

    material->model_matrices.clear();
    material->model_matrices.reserve(material->drawables.size());

    if (material->is_billboard)
    {
        for (auto const& drawable : material->drawables)
        {
            drawable->entity->transform->set_euler_angles(glm::vec3(0.0f, Camera::get_main_camera()->entity->transform->get_euler_angles().y, 0.0f));
        }
    }

    // TODO: Adjust bounding boxes on GPU?
    for (uint32_t i = 0; i < material->drawables.size(); ++i)
    {
        if (material->drawables[i]->entity->transform->needs_bounding_box_adjusting)
        {
            material->drawables[i]->bounds = material->first_drawable->get_adjusted_bounding_box(material->drawables[i]->entity->transform->get_model_matrix());
            material->bounding_boxes[i] = BoundingBoxShader(material->drawables[i]->bounds);
            material->drawables[i]->entity->transform->needs_bounding_box_adjusting = false;
        }
    }

    perform_frustum_culling(material);

    shader->use();

    //set_shader_uniforms(shader, projection_view, projection_view_no_translation);

    shader->set_vec3("material.color", glm::vec3(first_drawable->material->color.x, first_drawable->material->color.y, first_drawable->material->color.z));
    shader->set_float("material.specular", first_drawable->material->specular);
    shader->set_float("material.shininess", first_drawable->material->shininess);

    first_drawable->draw_instanced(material->model_matrices.size());
}

void Renderer::set_shader_uniforms(std::shared_ptr<Shader> const& shader, glm::mat4 const& projection_view, glm::mat4 const& projection_view_no_translation) const
{
    // TODO: Ultimately we would probably want to cache the uniform location instead of retrieving them by name

    shader->set_vec3("cameraPosition", Camera::get_main_camera()->get_position());
    shader->set_mat4("PV", projection_view);
    shader->set_mat4("PVnoTranslation", projection_view_no_translation);

    // TODO: Choose only the closest lights

    int32_t enabled_light_count = 0;
    for (uint32_t i = 0; i < point_lights.size(); ++i)
    {
        if (!point_lights[i]->enabled)
            continue;

        std::string light_element = std::format("pointLights[{}].", enabled_light_count);
        shader->set_vec3(light_element + "position", point_lights[i]->entity->transform->get_local_position());

        shader->set_vec3(light_element + "ambient", point_lights[i]->ambient);
        shader->set_vec3(light_element + "diffuse", point_lights[i]->diffuse);
        shader->set_vec3(light_element + "specular", point_lights[i]->specular);

        shader->set_float(light_element + "constant", point_lights[i]->constant);
        shader->set_float(light_element + "linear", point_lights[i]->linear);
        shader->set_float(light_element + "quadratic", point_lights[i]->quadratic);

        enabled_light_count++;
    }

    shader->set_int("pointLightCount", enabled_light_count > max_point_lights ? max_point_lights : enabled_light_count);

    enabled_light_count = 0;
    for (uint32_t i = 0; i < spot_lights.size(); ++i)
    {
        if (!spot_lights[i]->enabled)
            continue;

        std::string light_element = std::format("spotLights[{}].", enabled_light_count);
        shader->set_vec3(light_element + "position", spot_lights[i]->entity->transform->get_local_position());
        shader->set_vec3(light_element + "direction", spot_lights[i]->entity->transform->get_forward());

        shader->set_vec3(light_element + "ambient", spot_lights[i]->ambient);
        shader->set_vec3(light_element + "diffuse", spot_lights[i]->diffuse);
        shader->set_vec3(light_element + "specular", spot_lights[i]->specular);

        shader->set_float(light_element + "cutOff", spot_lights[i]->cut_off);
        shader->set_float(light_element + "outerCutOff", spot_lights[i]->outer_cut_off);

        shader->set_float(light_element + "constant", spot_lights[i]->constant);
        shader->set_float(light_element + "linear", spot_lights[i]->linear);
        shader->set_float(light_element + "quadratic", spot_lights[i]->quadratic);

        enabled_light_count++;
    }

    shader->set_int("spotLightCount", enabled_light_count > max_spot_lights ? max_spot_lights : enabled_light_count);

    bool const directional_light_on = directional_light != nullptr && directional_light->enabled;
    if (directional_light_on)
    {
        shader->set_vec3("directionalLight.direction", directional_light->entity->transform->get_forward());

        shader->set_vec3("directionalLight.ambient", directional_light->ambient);
        shader->set_vec3("directionalLight.diffuse", directional_light->diffuse);
        shader->set_vec3("directionalLight.specular", directional_light->specular);
    }

    shader->set_bool("directionalLightOn", directional_light_on);
}
