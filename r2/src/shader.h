#pragma once

#if defined(R2_BACKEND_D3D11)
constexpr const char vs_source[] = R"(
cbuffer vertexBuffer : register(b0)
{
    float2 Resolution;
};

struct VS_INPUT
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = float4((input.pos.xy / Resolution) * 2.0 - 1.0, 0.0, 1.0);
    output.pos.y = -output.pos.y;
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}
)";
constexpr const char ps_source[] = R"(
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

sampler sampler0;
Texture2D texture0;

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv);
    return out_col;
}
)";
#elif defined(R2_BACKEND_OPENGL)
constexpr const char vs_source[] = R"(
#version 330 core

layout(std140) uniform ConstantBufferData 
{
    vec2 uResolution;
};

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec4 vColor;
out vec2 vUV;

void main()
{
    vec2 ndc = (aPos / uResolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
}
)";
constexpr const char ps_source[] = R"(
#version 130

in vec4 vColor;
in vec2 vUV;

uniform sampler2D uTexture0;

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture0, vUV);
    FragColor = vColor * texColor;
}
)";
#endif // backend
