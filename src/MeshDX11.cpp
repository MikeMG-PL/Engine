#include "MeshDX11.h"

#include <array>
#include <iostream>

#include "RendererDX11.h"

MeshDX11::MeshDX11(AK::Badge<MeshFactory>, std::vector<Vertex> const& vertices, std::vector<u32> const& indices,
                   std::vector<Texture> const& textures, DrawType const draw_type, std::shared_ptr<Material> const& material,
                   DrawFunctionType const draw_function) : Mesh(vertices, indices, textures, draw_type, material, draw_function)
{
    for (int i = 0; i < vertices.size(); ++i)
    {
        hack.emplace_back(vertices[i].position.x);
        hack.emplace_back(vertices[i].position.y);
        hack.emplace_back(vertices[i].position.z);
    }

    m_stride = sizeof(float) * 3;

    D3D11_BUFFER_DESC vertex_buffer_desc = {};
    vertex_buffer_desc.ByteWidth = hack.size() * sizeof(float);
    vertex_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA const vertex_subresource_data = { hack.data() };

    HRESULT const h_result = RendererDX11::get_instance_dx11()->get_device()->CreateBuffer(&vertex_buffer_desc, &vertex_subresource_data, &m_vertex_buffer);

    assert(SUCCEEDED(h_result));
}

MeshDX11::MeshDX11(MeshDX11&& mesh) noexcept : Mesh(mesh.vertices, mesh.indices, mesh.textures, mesh.m_draw_type, mesh.material, mesh.m_draw_function)
{
    m_vertex_buffer = mesh.m_vertex_buffer;
    mesh.m_vertex_buffer = nullptr;

    mesh.vertices.clear();
    mesh.indices.clear();
    mesh.textures.clear();
}

MeshDX11::~MeshDX11()
{
    m_vertex_buffer->Release();

    vertices.clear();
    indices.clear();
    textures.clear();
}

void MeshDX11::draw() const
{
    auto const device_context = RendererDX11::get_instance_dx11()->get_device_context();

    device_context->IASetPrimitiveTopology(m_primitive_topology);

    std::cout << "Draw" << "\n";
    for (auto const vert : vertices)
    {
        std::cout << glm::to_string(vert.position) << "\n";
    }

    device_context->IASetVertexBuffers(0, 1, &m_vertex_buffer, &m_stride, &m_offset);
    device_context->Draw(hack.size() / 3, 0);
}

void MeshDX11::draw(u32 const size, void const* offset) const
{
}

void MeshDX11::draw_instanced(i32 const size) const
{
}

void MeshDX11::bind_textures() const
{
}

void MeshDX11::unbind_textures() const
{
}
