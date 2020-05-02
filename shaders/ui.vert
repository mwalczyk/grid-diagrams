#version 460

uniform	int u_number_of_vertices;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

layout(location = 0) in vec3 i_position;

out VS_OUT
{
    vec3 color;
} vs_out;

vec3 hsv_to_rgb(in vec3 c) 
{
  vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
  return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
}

void main() 
{
    gl_Position = u_projection * u_view * u_model * vec4(i_position, 1.0);

    // Convert the vertex ID to a unique color
    vs_out.color = hsv_to_rgb(vec3(float(gl_VertexID) / u_number_of_vertices, 1.0, 1.0));
}