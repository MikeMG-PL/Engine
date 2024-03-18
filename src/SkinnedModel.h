#pragma once
#include <string>
#include <vector>

#include <assimp/material.h>

#include "AnimXForm.h"
#include "Rig.h"

struct aiMaterial;
struct aiMesh;
struct aiScene;
struct aiNode;

#include "Texture.h"
#include "Mesh.h"
#include "AK/Badge.h"

enum LoadMode
{
	RIG,
    ANIM
};

class SkinnedModel : public Drawable
{
public:
    static std::shared_ptr<SkinnedModel> create(std::string const& model_path, std::string const& anim_path, std::shared_ptr<Material> const& material);
    static std::shared_ptr<SkinnedModel> create(std::shared_ptr<Material> const& material);

    // For convenience while the animation system is being developed
    Rig rig;

    // For debugging so we can test skinning using sin(time)
    float time = 0;

    std::vector<xform> model_pose;
    std::vector<xform> local_pose;

    SkinnedModel() = delete;
    explicit SkinnedModel(AK::Badge<SkinnedModel>, std::string const& model_path, std::string const& anim_path, std::shared_ptr<Material> const& material);
    explicit SkinnedModel(AK::Badge<SkinnedModel>, std::shared_ptr<Material> const& material);

    virtual std::string get_name() const override;

    void pre_draw_update(); // For skinning purposes
    virtual void draw() const override;

    virtual void draw_instanced(i32 const size) override;

    virtual void prepare();
    virtual void reset();
    virtual void reprepare();

    virtual void calculate_bounding_box() override;
    virtual void adjust_bounding_box() override;
    virtual BoundingBox get_adjusted_bounding_box(glm::mat4 const& model_matrix) const override;

protected:
    explicit SkinnedModel(std::string const& model_path, std::shared_ptr<Material> const& material);
    explicit SkinnedModel(std::shared_ptr<Material> const& material);

    DrawType m_draw_type = DrawType::Triangles;
    std::vector<std::shared_ptr<Mesh>> m_meshes = {};

private:

    void load_model(std::string const& path, LoadMode mode);
    void proccess_node(aiNode const* node);
    std::shared_ptr<Mesh> proccess_mesh(aiMesh const* mesh);
    std::vector<Texture> load_material_textures(aiMaterial const* material, aiTextureType type, TextureType const type_name);

    // Skinning
    void extract_bone_data(aiNode* node, LoadMode mode);
    void extract_bone_data_from_mesh(aiMesh* mesh, LoadMode mode);

    const aiScene* m_scene;
    std::string m_directory;
    std::string m_model_path;
    std::string m_anim_path;
    std::vector<Texture> m_loaded_textures;

    friend class SceneSerializer;
};
