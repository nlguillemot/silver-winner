#ifndef COMMON_HLSL
#define COMMON_HLSL

struct PerCameraData
{
    float4x4 WorldViewProjection;
    float4 WorldPosition;
};

struct PerMaterialData
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Shininess; // all same
    float4 Opacity; // all same

    float4 HasDiffuse; // all same
    float4 HasSpecular; // all same
    float4 HasBump; // all same
};

struct PerSceneNodeData
{
    float4x4 WorldTransform;
    float4x4 NormalTransform;
};

#define CAMERA_BUFFER_SLOT 0
#define MATERIAL_BUFFER_SLOT 1
#define SCENENODE_BUFFER_SLOT 2

#define DIFFUSE_TEXTURE_SLOT 0
#define SPECULAR_TEXTURE_SLOT 1
#define BUMP_TEXTURE_SLOT 2

#define DIFFUSE_SAMPLER_SLOT 0
#define SPECULAR_SAMPLER_SLOT 1
#define BUMP_SAMPLER_SLOT 2

#define BUFFER_REGISTER(slot) register(b##slot)
#define TEXTURE_REGISTER(slot) register(t##slot)
#define SAMPLER_REGISTER(slot) register(s##slot)

#endif // COMMON_HLSL