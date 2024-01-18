#include "Material.h"

#include "Renderer.h"

std::shared_ptr<Material> Material::create(std::shared_ptr<Shader> const& shader, int32_t const render_order, bool const is_gpu_instanced, bool const is_billboard)
{
    auto material = std::make_shared<Material>(shader, render_order, is_gpu_instanced, is_billboard);

    Renderer::get_instance()->register_material(material);
    shader->materials.emplace_back(material);

    return material;
}

Material::Material(std::shared_ptr<Shader> const& shader, int32_t const render_order, bool const is_gpu_instanced, bool const is_billboard)
    : shader(shader), is_billboard(is_billboard), is_gpu_instanced(is_gpu_instanced), render_order(render_order)
{
}

int32_t Material::get_render_order() const
{
    return render_order;
}
