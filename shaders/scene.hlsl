#include "common.hlsl"

struct VSIn
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
};

struct VSOut
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

struct PSOut
{
    float4 Color : SV_Target;
};

cbuffer CameraBuffer : BUFFER_REGISTER(CAMERA_BUFFER_SLOT)
{
    PerCameraData Camera;
};

cbuffer MaterialBuffer : BUFFER_REGISTER(MATERIAL_BUFFER_SLOT)
{
    PerMaterialData Material;
};

cbuffer SceneNodeBuffer : BUFFER_REGISTER(SCENENODE_BUFFER_SLOT)
{
    PerSceneNodeData SceneNode;
};

Texture2D DiffuseTexture : TEXTURE_REGISTER(DIFFUSE_TEXTURE_SLOT);
SamplerState DiffuseSampler : SAMPLER_REGISTER(DIFFUSE_SAMPLER_SLOT);

VSOut VSmain(VSIn input)
{
    VSOut output;
    output.Position = mul(mul(input.Position, SceneNode.WorldTransform), Camera.WorldViewProjection);
    output.TexCoord = input.TexCoord;
    return output;
}

PSOut PSmain(VSOut input)
{
    PSOut output;
    output.Color = DiffuseTexture.Sample(DiffuseSampler, input.TexCoord);
    return output;
}