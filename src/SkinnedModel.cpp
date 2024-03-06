#include "SkinnedModel.h"

#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glad/glad.h>
#include <map>

#include "aiHelpers.h"
#include "Entity.h"
#include "Mesh.h"
#include "MeshFactory.h"
#include "Texture.h"
#include "TextureLoader.h"
#include "Vertex.h"
#include "AK/Types.h"

std::shared_ptr<SkinnedModel> SkinnedModel::create(std::string const& model_path, std::string const& anim_path, std::shared_ptr<Material> const& material)
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

SkinnedModel::SkinnedModel(AK::Badge<SkinnedModel>, std::string const& model_path, std::string const& anim_path, std::shared_ptr<Material> const& material)
    : Drawable(material), m_model_path(model_path)
{
}

SkinnedModel::SkinnedModel(AK::Badge<SkinnedModel>, std::shared_ptr<Material> const& material) : Drawable(material)
{
}

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

SkinnedModel::SkinnedModel(std::string const& model_path, std::shared_ptr<Material> const& material)
    : Drawable(material), m_model_path(model_path)
{
}

SkinnedModel::SkinnedModel(std::shared_ptr<Material> const& material) : Drawable(material)
{
}

std::string SkinnedModel::get_name() const
{
    std::string const name = typeid(decltype(*this)).name();
    return name.substr(6);
}

void SkinnedModel::draw() const
{
    for (auto const& mesh : m_meshes)
        mesh->draw();
}

void SkinnedModel::draw_instanced(i32 const size)
{
    for (auto const& mesh : m_meshes)
        mesh->draw_instanced(size);
}

void SkinnedModel::prepare()
{
    if (m_material->is_gpu_instanced)
    {
        if (m_material->first_drawable != nullptr)
            return;

        m_material->first_drawable = std::dynamic_pointer_cast<Drawable>(shared_from_this());
    }

    load_model(m_model_path, RIG);
    load_model(m_model_path, ANIM);
}

void SkinnedModel::reset()
{
    m_meshes.clear();
    m_loaded_textures.clear();
}

void SkinnedModel::reprepare()
{
    SkinnedModel::reset();
    SkinnedModel::prepare();
}

void SkinnedModel::load_model(std::string const& path, LoadMode mode)
{
    Assimp::Importer importer;
    aiScene const* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_PopulateArmatureData);

    if ((!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) && mode == RIG)
    {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << "::LOADMODE::" << mode << std::endl;
        return;
    }
    if (mode == ANIM)
    {
        if (!scene || !scene->HasAnimations())
        {
            std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << "::LOADMODE::" << mode << std::endl;
            return;
        }
    }

    m_directory = path.substr(0, path.find_last_of('/'));

    if (mode == RIG)
    {
        // Extract rig data from the scene
        extractBoneData(scene->mRootNode, mode);

        // Process meshes and use the extracted bone data
        proccess_node(scene->mRootNode);
    }
    if (mode == ANIM) // If we are loading animation, not rig
    {
        // Just a pose for now
        // TODO: Cut out to separate function
        // TODO: Attach sampler of course lol

        std::cout << rig.numBones << std::endl;

        for (int i = 0; i < rig.numBones; i++)
        {
            xform transform;

            // const aiAnimation* animation = scene->mAnimations[0]; // Get the first animation
            // const aiNodeAnim* channel = animation->mChannels[i]; // Get the first channel
            //
            // // Get the position, rotation, and scaling keyframes from the first keyframe
            // const aiVector3D position = channel->mPositionKeys[0].mValue;
            // const aiQuaternion rotation = channel->mRotationKeys[0].mValue;
            //
            // transform.position = aiPosToGLMVec3(position);
            // transform.rotation = aiQuatToGLMQuat(rotation);

            localPose.emplace_back(transform);
        }
    }

    proccess_node(scene->mRootNode);
}

void SkinnedModel::proccess_node(aiNode const* node)
{
    for (u32 i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh const* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.emplace_back(proccess_mesh(mesh));
    }

    for (u32 i = 0; i < node->mNumChildren; ++i)
    {
        proccess_node(node->mChildren[i]);
    }
}

std::shared_ptr<Mesh> SkinnedModel::proccess_mesh(aiMesh const* mesh)
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<Texture> textures;

    glm::ivec4 skinIndices = { -1, -1, -1, -1 };
    glm::vec4 skinWeights = { 0, 0, 0, 0 };

    std::map<unsigned int, glm::ivec4> vertexToBoneIndices;
    std::map<unsigned int, glm::vec4> vertexToBoneWeights;
    Vertex vertex;
    unsigned int vertexID = 0;
    unsigned int boneID = 0;
    float weight = 0.0f;

    // TODO: Optimize, reduce number of for loops used. Now I just want to make it work.

    for (int i = 0; i < mesh->mNumBones; i++)
    {
        for (int j = 0; j < mesh->mBones[i]->mNumWeights; j++)
        {
            vertexID = mesh->mBones[i]->mWeights[j].mVertexId;
            boneID = i;
            weight = mesh->mBones[i]->mWeights[j].mWeight;

            // Initialize the ivec4 with -1s and vec4 with 0s - default values

            if (!vertexToBoneIndices.contains(vertexID))
            {
                vertexToBoneIndices.try_emplace(vertexID, skinIndices);
                vertexToBoneWeights.try_emplace(vertexID, skinWeights);
            }

            // TODO: Get rid of this way of filling the ivec4.
            if (vertexToBoneIndices[vertexID].x == -1)
            {
                vertexToBoneIndices[vertexID].x = i;
                vertexToBoneWeights[vertexID].x = weight;
            }
            else if (vertexToBoneIndices[vertexID].y == -1)
            {
                vertexToBoneIndices[vertexID].y = i;
                vertexToBoneWeights[vertexID].y = weight;
            }
            else if (vertexToBoneIndices[vertexID].z == -1)
            {
                vertexToBoneIndices[vertexID].z = i;
                vertexToBoneWeights[vertexID].z = weight;
            }
            else if (vertexToBoneIndices[vertexID].w == -1)
            {
                vertexToBoneIndices[vertexID].w = i;
                vertexToBoneWeights[vertexID].w = weight;
            }
        }
    }

    for (u32 i = 0; i < mesh->mNumVertices; ++i)
    {
        // Process vertex positions, normals, texture coordinates
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

        vertex.skinIndices = vertexToBoneIndices[i];
        vertex.skinWeights = vertexToBoneWeights[i];

        vertices.push_back(vertex);
    }

    auto x = vertexToBoneIndices;
    auto x1 = vertexToBoneWeights;

    for (u32 i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace const face = mesh->mFaces[i];
        for (u32 k = 0; k < face.mNumIndices; k++)
        {
            indices.push_back(face.mIndices[k]);
        }
    }

    aiMaterial const* assimp_material = scene->mMaterials[mesh->mMaterialIndex];

    std::vector<Texture> diffuse_maps = load_material_textures(assimp_material, aiTextureType_DIFFUSE, TextureType::Diffuse);
    textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());

    std::vector<Texture> specular_maps = load_material_textures(assimp_material, aiTextureType_SPECULAR, TextureType::Specular);
    textures.insert(textures.end(), specular_maps.begin(), specular_maps.end());

    return MeshFactory::create(vertices, indices, textures, m_draw_type, m_material);
}

void SkinnedModel::extractBoneData(aiNode* node, LoadMode mode)
{
    // Process each mesh node if it contains bones
    if (node->mNumMeshes > 0)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[0]]; // Assuming one mesh per node for simplicity
        extractBoneDataFromMesh(mesh, mode);
    }

    // Recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        extractBoneData(node->mChildren[i], mode);
    }
}

void SkinnedModel::extractBoneDataFromMesh(aiMesh* mesh, LoadMode mode)
{
    // Extract bone data from the mesh
    for (unsigned int i = 0; i < mesh->mNumBones; i++)
    {
        // Extract bone name, parent index, and bind pose
        const aiBone* bone = mesh->mBones[i];
        hstring parentBoneName;
        hstring boneName = hash(bone->mName.C_Str());

        if (bone->mNode == nullptr || bone->mNode->mParent == nullptr)
            parentBoneName = -1;
        else
            parentBoneName = hash(bone->mNode->mParent->mName.C_Str());

        // Find parent bone in the mBones array
        int parentIndex = -1;

        for (unsigned int j = 0; j < mesh->mNumBones; j++)
        {
            const hstring checkedBoneName = hash(mesh->mBones[j]->mName.C_Str());
            if (i != j && parentBoneName == checkedBoneName)
            {
                // Found the parent bone
                parentIndex = j;
                break;
            }
        }

        const aiMatrix4x4 aiInverseBindPose = bone->mOffsetMatrix;
        glm::mat4 inverseBindPose = aiMatrix4x4ToGlm(&aiInverseBindPose);

        rig.boneNames.push_back(boneName);
        rig.parents.push_back(parentIndex);
        rig.inverseBindPose.push_back(inverseBindPose);
    }
    rig.numBones += mesh->mNumBones;
}

std::vector<Texture> SkinnedModel::load_material_textures(aiMaterial const* material, aiTextureType const type, TextureType const type_name)
{
    std::vector<Texture> textures;

    u32 const material_count = material->GetTextureCount(type);
    for (u32 i = 0; i < material_count; ++i)
    {
        aiString str;
        material->GetTexture(type, i, &str);

        bool is_already_loaded = false;
        for (const auto& loaded_texture : m_loaded_textures)
        {
            if (std::strcmp(loaded_texture.path.data(), str.C_Str()) == 0)
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

        Texture texture = TextureLoader::get_instance()->load_texture(file_path, type_name);
        textures.push_back(texture);
        m_loaded_textures.push_back(texture);
    }

    return textures;
}
