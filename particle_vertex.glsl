#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 particleColor;

out vec3 Color;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    // Calculate distance from camera (in view space)
    vec4 viewPos = view * model * vec4(aPos, 1.0);
    float distance = length(viewPos.xyz);
    
    // Size particles based on distance (closer particles are larger)
    // and consistent with the new car scale
    gl_PointSize = max(8.0 * (1.0 / (distance * 0.1 + 0.1)), 2.0);
    
    // Pass color to fragment shader
    Color = particleColor;
}