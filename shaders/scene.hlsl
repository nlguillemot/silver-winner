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
    float3 WorldPosition : WORLDPOSITION;
    float2 TexCoord : TEXCOORD;
    float3 WorldNormal : WORLDNORMAL;
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

Texture2D SpecularTexture : TEXTURE_REGISTER(SPECULAR_TEXTURE_SLOT);
SamplerState SpecularSampler : SAMPLER_REGISTER(SPECULAR_SAMPLER_SLOT);

VSOut VSmain(VSIn input)
{
    VSOut output;
    output.WorldPosition = mul(input.Position, SceneNode.WorldTransform).xyz;
    output.Position = mul(float4(output.WorldPosition,1.0), Camera.WorldViewProjection);
    output.TexCoord = input.TexCoord;
    output.WorldNormal = mul(float4(input.Normal,0.0), SceneNode.NormalTransform).xyz;
    return output;
}

PSOut PSmain(VSOut input)
{
    PSOut output;
    float4 diffuseMap = DiffuseTexture.Sample(DiffuseSampler, input.TexCoord);
    float4 specularMap = float4(SpecularTexture.Sample(SpecularSampler, input.TexCoord).rrr, 0);

    float3 N = normalize(input.WorldNormal);
    float3 V = normalize(Camera.WorldPosition.xyz - input.WorldPosition);
    float3 L = V;
    float G = max(0, dot(N, L));
    float3 R = reflect(-L, N);
    float S = pow(max(0, dot(R, V)), Material.Shininess.r);

    float4 ambient = diffuseMap * Material.Ambient;
    float4 diffuse = diffuseMap * Material.Diffuse * G;
    float4 specular = specularMap * Material.Specular * S;
    
    output.Color = ambient + diffuse + specular;
    
    return output;
}