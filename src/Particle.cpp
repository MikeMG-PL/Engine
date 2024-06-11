#include "Particle.h"

#include "AK/AK.h"
#include "ConstantBufferTypes.h"
#include "Entity.h"
#include "Globals.h"
#include "RendererDX11.h"
#include "ResourceManager.h"
#include "ShaderFactory.h"
#include "Sprite.h"

#include <glm/gtc/type_ptr.inl>

std::shared_ptr<Particle> Particle::create()
{
    auto const particle_shader = ResourceManager::get_instance().load_shader("./res/shaders/particle.hlsl", "./res/shaders/particle.hlsl");
    auto const particle_material = Material::create(particle_shader);
    particle_material->casts_shadows = false;
    particle_material->needs_forward_rendering = true;
    particle_material->is_billboard = true; // !

    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

    auto particle = std::make_shared<Particle>(AK::Badge<Particle> {}, 1.0f, color, 1.0f, particle_material);
    return particle;
}

std::shared_ptr<Particle> Particle::create(float speed, glm::vec4 const& color, float spawn_bounds)
{
    auto const particle_shader = ResourceManager::get_instance().load_shader("./res/shaders/particle.hlsl", "./res/shaders/particle.hlsl");
    auto const particle_material = Material::create(particle_shader);
    particle_material->casts_shadows = false;
    particle_material->needs_forward_rendering = true;
    particle_material->is_billboard = true; // !

    auto particle = std::make_shared<Particle>(AK::Badge<Particle> {}, speed, color, spawn_bounds, particle_material);
    return particle;
}

Particle::Particle(AK::Badge<Particle>, float speed, glm::vec4 const& color, float spawn_bounds, std::shared_ptr<Material> const& mat)
    : Drawable(mat), m_particle_material(mat), m_color(color), m_speed(speed), m_spawn_bounds(spawn_bounds)
{
}

void Particle::initialize()
{
    Drawable::initialize();

    set_can_tick(true);

    entity->add_component(Sprite::create(m_particle_material, "./res/textures/particle.png"));

    entity->transform->set_scale({0.1f, 0.1f, 0.1f});
    entity->transform->set_local_position({AK::random_float(-m_spawn_bounds, m_spawn_bounds),
                                           AK::random_float(-m_spawn_bounds, m_spawn_bounds),
                                           AK::random_float(-m_spawn_bounds, m_spawn_bounds)});

    update_particle();
}

void Particle::draw() const
{
}

void Particle::draw_editor()
{
    Drawable::draw_editor();

    ImGui::ColorEdit4("Color", glm::value_ptr(m_color));
    update_particle();
}

void Particle::update_particle() const
{
    m_particle_material->color = m_color;
}

void Particle::update()
{
    move();
    decrement_alpha();
    update_particle();
}

void Particle::move() const
{
    glm::vec3 const p = entity->transform->get_position();
    entity->transform->set_position({p.x, p.y + delta_time * m_speed, p.z});
}

void Particle::decrement_alpha()
{
    m_color.a -= delta_time;

    if (m_color.a < 0.01f)
        entity->destroy_immediate();
}

bool Particle::is_particle() const
{
    return true;
}
