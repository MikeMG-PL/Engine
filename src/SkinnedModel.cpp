#include "SkinnedModel.h"

#include "AK/Math.h"
#include "AK/Types.h"
#include "Entity.h"
#include "Globals.h"
#include "Mesh.h"
#include "MeshFactory.h"
#include "Renderer.h"
#include "RendererDX11.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Vertex.h"

#include <filesystem>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#if EDITOR
#include <imgui.h>
#include <imgui_stdlib.h>
#endif

std::shared_ptr<SkinnedModel> SkinnedModel::create()
{
    auto model = std::make_shared<SkinnedModel>(AK::Badge<SkinnedModel> {}, default_material);

    return model;
}

std::shared_ptr<SkinnedModel> SkinnedModel::create(std::string const& model_path, std::string const& anim_path,
                                                   std::shared_ptr<Material> const& material)
{
    auto model = std::make_shared<SkinnedModel>(AK::Badge<SkinnedModel> {}, model_path, anim_path, material);
    model->prepare();

    return model;
}

std::shared_ptr<SkinnedModel> SkinnedModel::create(std::shared_ptr<Material> const& material)
{
    auto model = std::make_shared<SkinnedModel>(AK::Badge<SkinnedModel> {}, material);

    return model;
}

std::shared_ptr<SkinnedModel> SkinnedModel::create(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> const& material)
{
    auto model = std::make_shared<SkinnedModel>(AK::Badge<SkinnedModel> {}, material);

    model->m_meshes.emplace_back(mesh);

    return model;
}

SkinnedModel::SkinnedModel(AK::Badge<SkinnedModel>, std::string const& model_path, std::string const& anim_path,
                           std::shared_ptr<Material> const& material)
    : Drawable(material), model_path(model_path), anim_path(anim_path)
{
}

SkinnedModel::SkinnedModel(AK::Badge<SkinnedModel>, std::shared_ptr<Material> const& material) : Drawable(material)
{
}

#if EDITOR
void SkinnedModel::draw_editor()
{
    Drawable::draw_editor();

    ImGui::InputText("SkinnedModel Path", &model_path);

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        reprepare();
    }

    // Choose rasterizer draw mode for individual model
    std::array const draw_type_items = {"Default", "Wireframe", "Solid"};

    i32 current_item_index = static_cast<i32>(m_rasterizer_draw_type);
    if (ImGui::Combo("Rasterizer Draw Type", &current_item_index, draw_type_items.data(), draw_type_items.size()))
    {
        m_rasterizer_draw_type = static_cast<RasterizerDrawType>(current_item_index);
    }
}
#endif

void SkinnedModel::calculate_bounding_box()
{
    for (auto const& mesh : m_meshes)
        mesh->calculate_bounding_box();

    // TODO: Merge bounding boxes together
    if (!m_meshes.empty())
        bounds = m_meshes[0]->bounds;
}

void SkinnedModel::adjust_bounding_box()
{
    // TODO: If we merge bounding boxes together, I think we can just adjust the whole model bounding box
    for (auto const& mesh : m_meshes)
        mesh->adjust_bounding_box(entity->transform->get_model_matrix());

    if (!m_meshes.empty())
        bounds = m_meshes[0]->bounds;
}

BoundingBox SkinnedModel::get_adjusted_bounding_box(glm::mat4 const& model_matrix) const
{
    if (!m_meshes.empty())
        return m_meshes[0]->get_adjusted_bounding_box(model_matrix);

    return {};
}

bool SkinnedModel::is_skinned_model() const
{
    return true;
}

SkinnedModel::SkinnedModel(std::shared_ptr<Material> const& material) : Drawable(material)
{
}

void SkinnedModel::draw() const
{
    if (m_rasterizer_draw_type == RasterizerDrawType::None)
    {
        return;
    }

    // Either wireframe or solid for individual model
    Renderer::get_instance()->set_rasterizer_draw_type(m_rasterizer_draw_type);

    RendererDX11::get_instance_dx11()->set_skinning_buffer(skinning_matrices.data());

    for (auto const& mesh : m_meshes)
        mesh->draw();

    Renderer::get_instance()->restore_default_rasterizer_draw_type();
}

void SkinnedModel::draw_instanced(i32 const size)
{
    for (auto const& mesh : m_meshes)
        mesh->draw_instanced(size);
}

void SkinnedModel::prepare()
{
    if (material->is_gpu_instanced)
    {
        if (material->first_drawable != nullptr)
            return;

        material->first_drawable = std::dynamic_pointer_cast<Drawable>(shared_from_this());
    }

    load_model(model_path, anim_path);
}

void SkinnedModel::reset()
{
    m_meshes.clear();
    m_loaded_textures.clear();
}

void SkinnedModel::reprepare()
{
    reset();
    prepare();
}

void SkinnedModel::load_model(std::string const& path_to_model, std::string const& path_to_anim)
{
    Assimp::Importer importer;
    aiScene const* scene = importer.ReadFile(path_to_model, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_PopulateArmatureData);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr)
    {
        std::cout << "Error. Failed loading a model: " << importer.GetErrorString() << "\n";
        return;
    }

    std::filesystem::path const filesystem_path = path_to_model; // FINISHED HERE
    m_directory = filesystem_path.parent_path().string();

    proccess_node(scene->mRootNode, scene);
}

void SkinnedModel::proccess_node(aiNode const* node, aiScene const* scene)
{
    for (u32 i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh const* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.emplace_back(proccess_mesh(mesh, scene));
    }

    for (u32 i = 0; i < node->mNumChildren; ++i)
    {
        proccess_node(node->mChildren[i], scene);
    }
}

std::shared_ptr<Mesh> SkinnedModel::proccess_mesh(aiMesh const* mesh, aiScene const* scene)
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<std::shared_ptr<Texture>> textures;

    if (mesh->mNumBones > 0)
        populate_rig_data(mesh);

    for (u32 i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex = {};

        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals())
        {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }

        if (mesh->mTextureCoords[0] != nullptr)
        {
            vertex.texture_coordinates = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else
        {
            vertex.texture_coordinates = glm::vec2(0.0f, 0.0f);
        }

        for (u32 b = 0; b < mesh->mNumBones; b++)
            assign_weights_and_indices(*mesh->mBones[b], vertex, i);

        vertices.push_back(vertex);
    }

    // Filling vertices with weights and indices

    for (u32 i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace const face = mesh->mFaces[i];
        for (u32 k = 0; k < face.mNumIndices; k++)
        {
            indices.push_back(face.mIndices[k]);
        }
    }

    aiMaterial const* assimp_material = scene->mMaterials[mesh->mMaterialIndex];

    std::vector<std::shared_ptr<Texture>> diffuse_maps =
        load_material_textures(assimp_material, aiTextureType_DIFFUSE, TextureType::Diffuse);
    textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());

    std::vector<std::shared_ptr<Texture>> specular_maps =
        load_material_textures(assimp_material, aiTextureType_SPECULAR, TextureType::Specular);
    textures.insert(textures.end(), specular_maps.begin(), specular_maps.end());

    return ResourceManager::get_instance().load_mesh(m_meshes.size(), model_path, vertices, indices, textures, m_draw_type, material);
}

std::vector<std::shared_ptr<Texture>> SkinnedModel::load_material_textures(aiMaterial const* material, aiTextureType const type,
                                                                           TextureType const type_name)
{
    std::vector<std::shared_ptr<Texture>> textures;

    u32 const material_count = material->GetTextureCount(type);
    for (u32 i = 0; i < material_count; ++i)
    {
        aiString str;
        material->GetTexture(type, i, &str);

        bool is_already_loaded = false;
        for (auto const& loaded_texture : m_loaded_textures)
        {
            if (std::strcmp(loaded_texture->path.data(), str.C_Str()) == 0)
            {
                textures.push_back(loaded_texture);
                is_already_loaded = true;
                break;
            }
        }

        if (is_already_loaded)
            continue;

        auto file_path = std::string(str.C_Str());
        file_path = m_directory + '/' + file_path;

        TextureSettings settings = {};
        settings.flip_vertically = false;
        settings.filtering_min = TextureFiltering::Nearest;
        settings.filtering_max = TextureFiltering::Nearest;
        settings.filtering_mipmap = TextureFiltering::Nearest;

        std::shared_ptr<Texture> texture = ResourceManager::get_instance().load_texture(file_path, type_name, settings);
        textures.push_back(texture);
        m_loaded_textures.push_back(texture);
    }

    return textures;
}

void SkinnedModel::populate_rig_data(aiMesh const* mesh)
{
    // Set number of bones
    m_rig.num_bones = mesh->mNumBones;

    // Generating bone IDs
    // std::unordered_map<aiBone*, u32> bones_ids = {};
    for (u32 i = 0; i < mesh->mNumBones; i++)
    {
        // bones_ids[mesh->mBones[i]] = i;
        bone_names_to_ids[mesh->mBones[i]->mName.C_Str()] = static_cast<i32>(i);
    }

    // for (aiBone const* bone : mesh->mBones)
    for (u32 i = 0; i < mesh->mNumBones; i++)
    {
        auto const bone = mesh->mBones[i];

        // Fill with bone names
        m_rig.bone_names.emplace_back(bone->mName.C_Str());

        // Decompose bind pose (reference pose, T-pose) and assign it
        aiQuaternion q = {1.0f, 0.0f, 0.0f, 0.0f};
        aiVector3D v = {0.0f, 0.0f, 0.0f};
        bone->mNode->mTransformation.DecomposeNoScaling(q, v);

        AK::xform bone_xform = {};

        bone_xform.pos = {v.x, v.y, v.z};
        bone_xform.rot = {q.w, q.x, q.y, q.z};

        m_rig.ref_pose.emplace_back(bone_xform);

        // Parent test
        for (unsigned int j = 0; j < mesh->mNumBones; j++)
        {
            // If currently checked j-th bone is the one processed in "for (aiBone const* bone : mesh->mBones)", find its ID and assign as bone's parent ID.
            if (std::string(mesh->mBones[j]->mName.C_Str()) == std::string(bone->mNode->mParent->mName.C_Str()))
            {
                // u32 const id = bones_ids[mesh->mBones[j]];
                i32 const id = bone_names_to_ids[mesh->mBones[j]->mName.C_Str()];
                m_rig.parents.emplace_back(id);
                break;
            }
        }
        // If no parent, it's root.
        i32 const id = -1;
        m_rig.parents.emplace_back(id);

        // FOR DEBUGGING:
        local_pose.emplace_back(bone_xform);
    }

    // Fill model_pose with local_pose matrices to prevent from being empty
    model_pose = local_pose;

    // Local Space --> Model Space
    m_rig.local_to_model(local_pose, model_pose);

    for (u32 i = 0; i < m_rig.num_bones; i++)
    {
        skinning_matrices.emplace_back(AK::Math::xform_to_mat4(model_pose[i]));
    }
}

void SkinnedModel::assign_weights_and_indices(aiBone const& bone, Vertex& vertex, u32 const processed_index)
{
    for (u32 i = 0; i < bone.mNumWeights; i++)
    {
        auto const weight_and_id = bone.mWeights[i];

        u32 const vertex_id = weight_and_id.mVertexId;

        if (vertex_id != processed_index)
            continue;

        float const vertex_weight = weight_and_id.mWeight;

        for (u8 j = 0; j < 4; j++)
        {
            if (vertex.skin_indices[j] == -1)
            {
                vertex.skin_indices[j] = bone_names_to_ids[bone.mName.C_Str()];
                vertex.skin_weights[j] = vertex_weight;
            }
        }
    }
}
