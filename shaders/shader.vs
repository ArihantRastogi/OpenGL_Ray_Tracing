#version 330
in vec3 Position;

uniform mat4 gWorld;      // Model matrix
uniform mat4 gView;       // View matrix
uniform mat4 gProjection; // Projection matrix

// Add output color for fragment shader
out vec4 vertexColor;

void main()
{
    // Apply full MVP transformation
    gl_Position = gProjection * gView * gWorld * vec4(Position, 1.0);
    
    // Create a color based on the normalized position (for visualizing the model)
    vertexColor = vec4(0.5 + 0.5 * normalize(Position), 1.0);
}

