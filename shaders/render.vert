#version 460

uniform mat4 u_light_space_matrix;
uniform float u_time;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

// The size (xyz) of the bounding box of this knot
uniform vec3 u_size_of_bounds;

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_color;
layout(location = 2) in vec2 i_texture_coordinates;

out VS_OUT
{
    vec3 color;
    vec4 light_space_position;
} vs_out;


void main() 
{
    gl_Position = u_projection * u_view * u_model * vec4(i_position, 1.0);

    vs_out.color = (i_position / u_size_of_bounds) * 0.5 + 0.5;
    vs_out.light_space_position = u_light_space_matrix * u_model * vec4(i_position, 1.0);
}