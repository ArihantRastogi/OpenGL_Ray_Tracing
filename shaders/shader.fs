#version 330
layout(location = 0) out vec4 diffuseColor;

// Add input color from vertex shader
in vec4 vertexColor;

void main()
{
    // Use the color passed from the vertex shader
    diffuseColor = vertexColor;
}
