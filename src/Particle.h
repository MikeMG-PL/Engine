#pragma once

#include "Drawable.h"
#include "GBuffer.h"

class Particle final : public Drawable
{
public:
    static std::shared_ptr<Particle> create();
    static std::shared_ptr<Particle> create(float speed, glm::vec4 const& color, float spawn_bounds);
    explicit Particle(AK::Badge<Particle>, float speed, glm::vec4 const& color, float spawn_bounds, std::shared_ptr<Material> const& mat);

    virtual void initialize() override;

    virtual void update() override;

    virtual bool is_particle() const override;

    virtual void draw() const override;
    virtual void draw_editor() override;

private:
    void update_particle() const;

    std::shared_ptr<Material> m_particle_material = {};

    ID3D11Buffer* m_constant_buffer_particle = nullptr;

    glm::vec4 m_color = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_speed = 1.0f;
    float m_spawn_bounds = 1.0f;
};
