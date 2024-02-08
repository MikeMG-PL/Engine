#pragma once

#include "Camera.h"
#include "Component.h"
#include "Window.h"

class Entity;

class PlayerInput : public Component
{
public:
    std::shared_ptr<Entity> camera_entity;
    std::shared_ptr<Window> window;

    float player_speed = 5.0f;
    float camera_speed = 12.5f;

    virtual void awake() override;
    virtual void update() override;

    std::shared_ptr<Entity> player;
    std::shared_ptr<Entity> player_model;
    std::shared_ptr<Entity> player_head;
    std::shared_ptr<Entity> camera_parent;

private:
    void switch_input();
    void process_input();
    void process_terminator_input();
    void focus_callback(int const focused);
    void mouse_callback(double const x, double const y);

    std::shared_ptr<Camera> m_camera;
    glm::vec3 m_camera_euler_angles_terminator = glm::vec3(-8.5f, 0.0f, 0.0f);

    glm::dvec2 last_mouse_position = glm::dvec2(1600 / 2.0, 900.0 / 2.0);
    float yaw = 0.0f;
    float pitch = 10.0f;
    bool mouse_just_entered = true;

    double m_sensitivity = 0.1;

    bool terminator_mode = false;

    // Either "edit" or "game" mode
    bool game_mode = true;
};
