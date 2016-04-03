#include "common.hlsl"

struct VSIn
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
    uint VertexID : SV_VertexID;
};

struct VSOut
{
    float4 Position : SV_Position;
};

struct PSOut
{
    float4 Color : SV_Target;
};

cbuffer Camera : register(b0)
{
    PerCameraData Camera;
};

VSOut VSmain(VSIn input)
{
    VSOut output;
    if (input.VertexID % 3 == 0) output.Position = float4(0, 0, 0.5, 1);
    if (input.VertexID % 3 == 1) output.Position = float4(-1, 0, 0.5, 1);
    if (input.VertexID % 3 == 2) output.Position = float4(0, 1, 0.5, 1);
    // output.Position = mul(input.Position, Camera.WorldViewProjection);
    return output;
}

PSOut PSmain(VSOut input)
{
    PSOut output;
    output.Color = float4(1, 0, 0, 1);
    return output;
}