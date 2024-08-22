#include "lighting_calculations.hlsl"

cbuffer object_buffer : register(b0)
{
    float4x4 projection_view_model;
    float4x4 model;
    float4x4 projection_view;
};

cbuffer skinning_buffer : register(b4)
{
    float4x4 bones[512];
}

struct VS_Input
{
    float3 pos: POSITION;
    float3 normal : NORMAL;
    float2 UV : TEXCOORD0;
    int4 skin_indices : TEXCOORD1;
    float4 skin_weights : TEXCOORD2;
};

struct VS_Output
{
    float4 pixel_pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 world_pos : POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D obj_texture : register(t0);
SamplerState obj_sampler_state : register(s0);

VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    const float4 pos = float4(input.pos, 1.0f);
    const float4 norm = float4(input.normal, 0.0f);
    float4 pos_skinned = {0.0f, 0.0f, 0.0f, 0.0f};
    float4 norm_skinned = {0.0f, 0.0f, 0.0f, 0.0f};

    for(int i = 0; i < 4; i++)
    {
        if(input.skin_indices[i] >= 0)
        {
            const float4x4 bone = bones[input.skin_indices[i]];
            const float weight = input.skin_weights[i];

            pos_skinned += mul(mul(bone, pos), weight);
            norm_skinned += mul(mul(bone, norm), weight);
        }
    }

    pos_skinned.w = 1.0f;

    output.world_pos = mul(model, pos).xyz;
    output.UV = input.UV;
    output.normal = mul(input.normal, (float3x3)model);
    output.pixel_pos = mul(projection_view_model, pos_skinned);
    return output;
}

float4 ps_main(VS_Output input) : SV_TARGET
{
    float3 norm = normalize(input.normal);

    float3 view_dir = normalize(camera_pos.xyz - input.world_pos.xyz);
    float3 diffuse_texture = obj_texture.Sample(obj_sampler_state, input.UV).rgb;

    float3 result = calculate_directional_light(directional_light, norm, view_dir, diffuse_texture, input.world_pos, true);

    for (int point_light_index = 0; point_light_index < number_of_point_lights; point_light_index++)
    {
        result += calculate_point_light(point_lights[point_light_index], norm, input.world_pos.rgb, view_dir, diffuse_texture, point_light_index, true);
    }

    for (int spot_light_index = 0; spot_light_index < number_of_spot_lights; spot_light_index++)
    {
        result += calculate_spot_light(spot_lights[spot_light_index], norm, input.world_pos, view_dir, diffuse_texture, spot_light_index, true);
    }

    return gamma_correction(result);
}
