#version 430 core

layout(location=0)in vec3 PositionInput;
layout(location=1)in vec3 NormalInput;
layout(location=2)in vec2 TextureCoordinatesInput;
layout(location=3)in ivec4 skinIndices;
layout(location=4)in vec4 skinWeights;

out vec2 TextureCoordinatesVertex;
out vec3 FragmentPosition;
out vec3 NormalVertex;

uniform mat4 PVM;
uniform mat4 model;

layout(binding=0)uniform skinningBuffer
{
    mat4 bones[512];
}skin;

void main()
{
    vec4 pos = vec4(PositionInput, 1.0f);
    vec4 norm = vec4(NormalInput, 1.0f);
    vec4 posSkinned=vec4(PositionInput,1.f);// Initialize to default
    
    // Check if there are bones influencing the vertex
    bool hasInfluences=false;
    for(int i=0;i<4;i++)
    {
        if(skinIndices[i]>=0)
        {
            hasInfluences=true;
            break;
        }
    }
    
    if(hasInfluences)
    {   
        // Perform skinning calculations
        posSkinned=vec4(0.f,0.f,0.f,0.f);
        vec4 normSkinned=vec4(0.f,0.f,0.f,0.f);
        
        for(int i=0;i<4;i++)
        {
            if(skinIndices[i]>=0)
            {
                mat4 bone=skin.bones[skinIndices[i]];
                float weight=skinWeights[i];
                
                posSkinned+=(bone*pos)*weight;
                normSkinned+=(bone*norm)*weight;
            }
        }
        
        posSkinned.w=1.f;
    }
    
    gl_Position=PVM*vec4(PositionInput,1.);
    FragmentPosition=vec3(model*vec4(PositionInput,1.));
    
    // TODO: Do this on the CPU
    NormalVertex=mat3(transpose(inverse(model)))*NormalInput;
    TextureCoordinatesVertex=TextureCoordinatesInput;
}
