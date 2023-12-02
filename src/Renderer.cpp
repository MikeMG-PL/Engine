#include "Renderer.h"

#include "Camera.h"
#include "Entity.h"

std::shared_ptr<Renderer> Renderer::create()
{
    // We can't use make_shared here because the constructor is private.
    // https://stackoverflow.com/questions/56735974/access-private-constructor-from-public-static-member-function-using-shared-ptr-i
    auto renderer = std::shared_ptr<Renderer>(new Renderer());

    assert(instance == nullptr);

    set_instance(renderer);

    return renderer;
}

void Renderer::register_shader(std::shared_ptr<Shader> const& shader)
{
    if (!shaders_map.contains(shader))
    {
        shaders_map.insert(std::make_pair(shader, std::vector<std::shared_ptr<Drawable>> {}));
    }
}

void Renderer::unregister_shader(std::shared_ptr<Shader> const& shader)
{
    shaders_map.erase(shader);
}

void Renderer::register_drawable(std::shared_ptr<Drawable> const& drawable)
{
    assert(shaders_map.contains(drawable->material->shader));

    shaders_map[drawable->material->shader].emplace_back(drawable);
}

void Renderer::unregister_drawable(std::shared_ptr<Drawable> const& drawable)
{
    auto& v = shaders_map[drawable->material->shader];
    if (auto const it = std::ranges::find(v, drawable); it != v.end())
    {
        // NOTE: Swap with last and pop to avoid shifting other elements.
        std::swap(v.at(it - v.begin()), v.at(v.size() - 1));
        v.pop_back();
    }
}

void Renderer::render() const
{
    // Premultiply projection and view matrices
    glm::mat4 const projection_view = Camera::get_main_camera()->projection * Camera::get_main_camera()->get_view_matrix();

    for (const auto& [shader, drawables] : shaders_map)
    {
        shader->use();
        
        for (auto const& drawable : drawables)
        {
            // Could be beneficial to sort drawables per entities as well
            shader->set_mat4("PVM", projection_view * drawable->entity->transform->get_model_matrix());
            drawable->draw();
        }
    }
}