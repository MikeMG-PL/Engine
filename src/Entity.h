#pragma once

#include "Component.h"
#include "Drawable.h"
#include "MainScene.h"
#include "Transform.h"
#include "AK/Badge.h"

class Entity : public std::enable_shared_from_this<Entity>
{
public:
    explicit Entity(AK::Badge<Entity>, std::string const& name);
    static std::shared_ptr<Entity> create(std::string const& name = "Entity");
    static std::shared_ptr<Entity> create(std::string const& guid, std::string const& name);

    // Entity that is not tied to any scene
    static std::shared_ptr<Entity> create_internal(std::string const& name = "Entity");

    void destroy_immediate();

    template <class T>
    std::shared_ptr<T> add_component()
    {
        auto component = std::make_shared<T>();
        components.emplace_back(component);
        component->entity = shared_from_this();

        MainScene::get_instance()->add_component_to_start(component);

        // Initialization for internal components
        component->initialize();

        // TODO: Assumption that this entity belongs to the main scene
        // NOTE: Currently we treat manually assigning references in the Game initialization code as if someone would ex. drag a reference
        //       to an object in the Unity's inspector. This means that the Awake call of the constructed components should be called
        //       after all of these references were assigned, or more precisely, when the game has started. We do this manually, by
        //       gathering all the components in the components_to_awake vector and calling Awake on those when the game starts.
        //       If the component is constructed during the gameplay, we call the Awake immediately here.
        if (MainScene::get_instance()->is_running)
        {
            component->awake();
            component->has_been_awaken = true;

            component->on_enabled();
        }
        else
        {
            MainScene::get_instance()->add_component_to_awake(component);
        }

        return component;
    }

    template <class T>
    std::shared_ptr<T> add_component(std::shared_ptr<T> component)
    {
        components.emplace_back(component);
        component->entity = shared_from_this();

        MainScene::get_instance()->add_component_to_start(component);

        // Initialization for internal components
        component->initialize();

        // TODO: Assumption that this entity belongs to the main scene
        // NOTE: Currently we treat manually assigning references in the Game initialization code as if someone would ex. drag a reference
        //       to an object in the Unity's inspector. This means that the Awake call of the constructed components should be called
        //       after all of these references were assigned, or more precisely, when the game has started. We do this manually, by
        //       gathering all the components in the components_to_awake vector and calling Awake on those when the game starts.
        //       If the component is constructed during the gameplay, we call the Awake immediately here.
        if (MainScene::get_instance()->is_running)
        {
            component->awake();
            component->has_been_awaken = true;

            component->on_enabled();
        }
        else
        {
            MainScene::get_instance()->add_component_to_awake(component);
        }

        return component;
    }

    template <class T, typename... TArgs>
    std::shared_ptr<T> add_component(TArgs&&... args)
    {
        auto component = std::make_shared<T>(std::forward<TArgs>(args)...);
        components.emplace_back(component);
        component->entity = shared_from_this();

        MainScene::get_instance()->add_component_to_start(component);

        // Initialization for internal components
        component->initialize();

        // TODO: Assumption that this entity belongs to the main scene
        // NOTE: Currently we treat manually assigning references in the Game initialization code as if someone would ex. drag a reference
        //       to an object in the Unity's inspector. This means that the Awake call of the constructed components should be called
        //       after all of these references were assigned, or more precisely, when the game has started. We do this manually, by
        //       gathering all the components in the components_to_awake vector and calling Awake on those when the game starts.
        //       If the component is constructed during the gameplay, we call the Awake immediately here.
        if (MainScene::get_instance()->is_running)
        {
            component->awake();
            component->has_been_awaken = true;

            component->on_enabled();
        }
        else
        {
            MainScene::get_instance()->add_component_to_awake(component);
        }

        return component;
    }

    // Component that is not tied to any scene.
    // It will not be awaken, started or updated. Only initialized.
    template <class T>
    std::shared_ptr<T> add_component_internal(std::shared_ptr<T> component)
    {
        components.emplace_back(component);
        component->entity = shared_from_this();

        // Initialization for internal components
        component->initialize();

        return component;
    }

    template<typename T>
    std::shared_ptr<T> get_component()
    {
        for (auto const& component : components)
        {
            auto comp = std::dynamic_pointer_cast<T>(component);
            if (comp != nullptr)
                return comp;
        }

        return nullptr;
    }

    std::string name;
    std::string guid;
    size_t hashed_guid;
    std::shared_ptr<Transform> transform;
    std::vector<std::shared_ptr<Component>> components = {};

private:

    std::string m_parent_guid; // NOTE: Only for serialization

    friend class SceneSerializer;
};
