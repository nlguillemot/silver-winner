#ifndef COMMON_HLSL
#define COMMON_HLSL

struct PerCameraData
{
    float4x4 WorldViewProjection;
};

struct PerMaterialData
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Shininess; // all same
    float4 Opacity; // all same
};

struct PerSceneNodeData
{
    float4x4 WorldTransform;
};

#define CAMERA_BUFFER_SLOT 0
#define MATERIAL_BUFFER_SLOT 1
#define SCENENODE_BUFFER_SLOT 2

#define DIFFUSE_TEXTURE_SLOT 0

#define DIFFUSE_SAMPLER_SLOT 0

#define BUFFER_REGISTER(slot) register(b##slot)
#define TEXTURE_REGISTER(slot) register(t##slot)
#define SAMPLER_REGISTER(slot) register(s##slot)

#endif // COMMON_HLSL