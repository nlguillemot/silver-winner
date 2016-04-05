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

VSOut VSmain(VSIn input)
{
    VSOut output;
    output.Position = mul(mul(input.Position, SceneNode.WorldTransform), Camera.WorldViewProjection);
    return output;
}

PSOut PSmain(VSOut input)
{
    PSOut output;
    output.Color = float4(input.Position.www / 5000.0, 1.0);
    return output;
}