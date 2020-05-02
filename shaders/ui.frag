#version 460

layout(location = 0) out vec4 o_color;

in VS_OUT
{
    vec3 color;
} fs_in;

void main()
{             
    o_color = vec4(fs_in.color, 1.0);
}