#version 460 core

// Fullscreen triangle without a VAO/VBO using gl_VertexID.
// Positions cover entire screen and generate UVs in 0..1.
layout (location = 0) out vec2 vUV;

void main()
{
    const vec2 POS[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(POS[gl_VertexID], 0.0, 1.0);
    vUV = POS[gl_VertexID] * 0.5 + 0.5;
}
