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
    float depth : DEPTH;
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
    output.pos = float4((input.pos.xy / Resolution) * 2.0 - 1.0, input.depth, 1.0);
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
Texture2D texture0 : register(t0);
Texture2D depth0 : register(t1);

float4 main(PS_INPUT input) : SV_TARGET
{
    float depth = depth0.Sample(sampler0, input.uv);
    if (depth > input.depth) {
        discard;
    }
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
layout(location = 1) in float aDepth;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;

out vec4 vColor;
out vec2 vUV;
out float vDepth;

void main()
{
    vec2 ndc = (aPos / uResolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
    vDepth = aDepth;
}
)";
constexpr const char ps_source[] = R"(
#version 130

in vec4 vColor;
in vec2 vUV;
in float vDepth;

uniform sampler2D uTexture0;
uniform sampler2D uDepth0;

out vec4 FragColor;

void main()
{
    float depth = texture(uDepth0, vUV).r;
    if (depth > vDepth) {
        discard;
    }
    vec4 texColor = texture(uTexture0, vUV);
    FragColor = vColor * texColor;
}
)";
#endif // backend
