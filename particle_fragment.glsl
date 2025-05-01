#version 330 core
in vec3 Color;
out vec4 FragColor;

uniform float particleAlpha;

void main()
{
    // Calculate distance from center of point
    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
    float dist = dot(circCoord, circCoord);
    
    // Discard fragments outside the circle
    if (dist > 1.0) {
        discard;
    }
    
    // Smooth the edges
    float alpha = particleAlpha * (1.0 - smoothstep(0.7, 1.0, dist));
    
    // Output color with alpha
    FragColor = vec4(Color, alpha);
}