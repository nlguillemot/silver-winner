#include "common.hlsl"

struct VSIn
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
    //uint VertexID : SV_VertexID;
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
    output.Position = mul(input.Position, Camera.WorldViewProjection);
    return output;
}

PSOut PSmain(VSOut input)
{
    PSOut output;
    output.Color = float4(input.Position.www / 5000.0, 1.0);
    return output;
}