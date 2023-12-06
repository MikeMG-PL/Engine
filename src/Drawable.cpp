#include "Drawable.h"

#include "Entity.h"
#include "Renderer.h"

Drawable::Drawable(std::shared_ptr<MaterialInstance> const& material_instance)
{
    this->material_instance = material_instance;
}

Drawable::Drawable(std::shared_ptr<MaterialInstance> const& material_instance, int32_t const render_order) : render_order(render_order)
{
    this->material_instance = material_instance;
}

void Drawable::initialize()
{
    entity->drawables.emplace_back(this);

    Renderer::get_instance()->register_drawable(std::dynamic_pointer_cast<Drawable>(shared_from_this()));
}
