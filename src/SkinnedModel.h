#pragma once
#include <string>
#include <vector>

#include <assimp/material.h>

#include "AK/Badge.h"
#include "Mesh.h"
#include "Rig.h"
#include "Texture.h"

#include <unordered_map>

struct aiBone;
struct aiMaterial;
struct aiMesh;
struct aiScene;
struct aiNode;

class SkinnedModel : public Drawable
{
public:
    static std::shared_ptr<SkinnedModel> create();
    static std::shared_ptr<SkinnedModel> create(std::string const& model_path, std::string const& anim_path,
                                                std::shared_ptr<Material> const& material);
    static std::shared_ptr<SkinnedModel> create(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> const& material);
    static std::shared_ptr<SkinnedModel> create(std::shared_ptr<Material> const& material);

    explicit SkinnedModel(AK::Badge<SkinnedModel>, std::string const& model_path, std::string const& anim_path,
                          std::shared_ptr<Material> const& material);
    explicit SkinnedModel(AK::Badge<SkinnedModel>, std::shared_ptr<Material> const& material);

#if EDITOR
    virtual void draw_editor() override;
#endif

    virtual void draw() const override;

    virtual void draw_instanced(i32 const size) override;

    virtual void prepare();
    virtual void reset();
    virtual void reprepare() override;

    virtual void calculate_bounding_box() override;
    virtual void adjust_bounding_box() override;
    virtual BoundingBox get_adjusted_bounding_box(glm::mat4 const& model_matrix) const override;

    virtual bool is_skinned_model() const override;

    std::string model_path = "./res/models/enemy/enemy.gltf";
    std::string anim_path = "./res/models/enemy/enemy.gltf";

protected:
    explicit SkinnedModel(std::shared_ptr<Material> const& material);

    DrawType m_draw_type = DrawType::Triangles;
    std::vector<std::shared_ptr<Mesh>> m_meshes = {};

private:
    void load_model(std::string const& path_to_model, std::string const& path_to_anim);
    void proccess_node(aiNode const* node, aiScene const* scene);
    std::shared_ptr<Mesh> proccess_mesh(aiMesh const* mesh, aiScene const* scene);
    std::vector<std::shared_ptr<Texture>> load_material_textures(aiMaterial const* material, aiTextureType type,
                                                                 TextureType const type_name);

    void populate_rig_data(aiMesh const* mesh);
    void assign_weights_and_indices(aiBone const& bone, Vertex& vertex, u32 const processed_index);

    Rig m_rig = {};
    std::unordered_map<std::string, i32> bone_names_to_ids = {};

    std::vector<AK::xform> local_pose = {};
    std::vector<AK::xform> model_pose = {};

    std::vector<glm::mat4> skinning_matrices = {};

    std::string m_directory = "";
    std::vector<std::shared_ptr<Texture>> m_loaded_textures = {};
};
