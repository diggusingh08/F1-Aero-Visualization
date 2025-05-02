#version 330 core
in vec3 lineColor;

out vec4 FragColor;

uniform float lineAlpha;

void main()
{
    FragColor = vec4(lineColor, lineAlpha);
}