#include "ShaderDX11.h"

#include <array>
#include <d3d11.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <iostream>

#include "Renderer.h"
#include "RendererDX11.h"

ShaderDX11::ShaderDX11(AK::Badge<ShaderFactory>, std::string const& compute_path) : Shader(compute_path)
{
}

ShaderDX11::ShaderDX11(AK::Badge<ShaderFactory>, std::string const& vertex_path, std::string const& fragment_path)
    : Shader(vertex_path, fragment_path)
{
    load_shader();
}

ShaderDX11::ShaderDX11(AK::Badge<ShaderFactory>, std::string const& vertex_path, std::string const& fragment_path,
                       std::string const& geometry_path)
    : Shader(vertex_path, fragment_path, geometry_path)
{
}

ShaderDX11::ShaderDX11(AK::Badge<ShaderFactory>, std::string const& vertex_path, std::string const& tessellation_control_path,
                       std::string const& tessellation_evaluation_path, std::string const& fragment_path)
    : Shader(vertex_path, tessellation_control_path, tessellation_evaluation_path, fragment_path)
{
}

void ShaderDX11::load_shader()
{
    // Load vertex shader
    ID3DBlob* vs_blob;
    {
        ID3DBlob* shader_compile_errors_blob;

        std::wstring const vertex_path_final = std::wstring(m_vertex_path.begin(), m_vertex_path.end());
        HRESULT hr = D3DCompileFromFile(vertex_path_final.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", 0, 0,
                                        &vs_blob, &shader_compile_errors_blob);

        if (FAILED(hr))
        {
            char const* error_string = nullptr;
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                error_string = "Error. Vertex shader file not found.";
            }
            else if (shader_compile_errors_blob)
            {
                error_string = static_cast<char const*>(shader_compile_errors_blob->GetBufferPointer());
                shader_compile_errors_blob->Release();
            }

            std::cout << error_string << "\n";
            return;
        }

        hr = RendererDX11::get_instance_dx11()->get_device()->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                                                                 nullptr, &m_vertex_shader);

        if (FAILED(hr))
        {
            std::cout << "Error. Vertex shader creation failed."
                      << "\n";
            vs_blob->Release();
            return;
        }
    }

    // Load pixel shader
    ID3DBlob* ps_blob;
    {
        ID3DBlob* shader_compile_errors_blob;

        std::wstring const pixel_path_final = std::wstring(m_fragment_path.begin(), m_fragment_path.end());
        HRESULT hr = D3DCompileFromFile(pixel_path_final.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", 0, 0,
                                        &ps_blob, &shader_compile_errors_blob);

        if (FAILED(hr))
        {
            char const* error_string = nullptr;

            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                error_string = "Error. Fragment shader file not found.";
            }
            else if (shader_compile_errors_blob)
            {
                error_string = static_cast<char const*>(shader_compile_errors_blob->GetBufferPointer());
                shader_compile_errors_blob->Release();
            }

            std::cout << error_string << "\n";
            return;
        }

        hr = RendererDX11::get_instance_dx11()->get_device()->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
                                                                                nullptr, &m_pixel_shader);

        if (FAILED(hr))
        {
            std::cout << "Error. Fragment shader creation failed."
                      << "\n";
            ps_blob->Release();
            return;
        }
    }

    {
        std::array<D3D11_INPUT_ELEMENT_DESC, 3> constexpr input_element_desc = {
            {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
             {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
             {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}}};

        HRESULT const hr = RendererDX11::get_instance_dx11()->get_device()->CreateInputLayout(
            input_element_desc.data(), input_element_desc.size(), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &m_input_layout);
        assert(SUCCEEDED(hr));
        vs_blob->Release();
    }
}

void ShaderDX11::use() const
{
    auto const instance = RendererDX11::get_instance_dx11();
    instance->get_device_context()->IASetInputLayout(m_input_layout);
    instance->get_device_context()->VSSetShader(m_vertex_shader, nullptr, 0);
    instance->get_device_context()->PSSetShader(m_pixel_shader, nullptr, 0);
}

void ShaderDX11::set_bool(std::string const& name, bool const value) const
{
}

void ShaderDX11::set_int(std::string const& name, i32 const value) const
{
}

void ShaderDX11::set_float(std::string const& name, float const value) const
{
}

void ShaderDX11::set_vec3(std::string const& name, glm::vec3 const value) const
{
}

void ShaderDX11::set_vec4(std::string const& name, glm::vec4 const value) const
{
}

void ShaderDX11::set_mat4(std::string const& name, glm::mat4 const value) const
{
}

i32 ShaderDX11::attach(char const* path, i32 type) const
{
    return 0;
}
