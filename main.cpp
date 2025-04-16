#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"
#include "OFFReader.h"
#define GL_SILENCE_DEPRECATION

/********************************************************************/
/*   Variables */

char theProgramTitle[] = "Ray Tracing Demo";
int theWindowWidth = 800, theWindowHeight = 800;
int theWindowPositionX = 40, theWindowPositionY = 40;
bool isFullScreen = false;
bool isAnimating = true;
float rotation = 0.0f;
GLuint VBO, VAO, IBO;
GLuint gWorldLocation;
GLuint gViewLocation;
GLuint gProjectionLocation;

// Ray tracing variables
GLuint rayTraceProgramID;
GLuint quadVAO, quadVBO;
bool useRayTracing = true;
bool enableShadows = true;
bool enableReflections = true;
int maxBounces = 3;
float reflectivity = 0.5f;

// Scene objects for ray tracing
struct RayTracingObject {
    int type;           // 0 = sphere, 1 = cube, 2 = mesh
    glm::vec3 position;
    glm::vec3 size;     // radius for sphere, half-size for cube
    glm::vec3 color;
    float reflectivity;
};

#define MAX_OBJECTS 16
RayTracingObject sceneObjects[MAX_OBJECTS];
int numObjects = 0;

// Light sources
struct Light {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

#define MAX_LIGHTS 4
Light lights[MAX_LIGHTS];
int numLights = 0;
glm::vec3 ambientLight(0.1f, 0.1f, 0.1f);

// Camera variables
glm::vec3 cameraPosition(7.0f, 7.0f, 7.0f);
glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
float cameraFOV = 45.0f;

OffModel* model = NULL;
int numVertices = 0;
int numIndices = 0;

/* Constants */
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";
const char *pRayTraceVSFileName = "shaders/quad.vs";
const char *pRayTraceFSFileName = "shaders/raytrace.fs";
char * offFilePath = "models/cube.off";

// Function declarations
static void AddShader(GLuint ShaderProgram, const char *pShaderText, GLenum ShaderType);

// Function to add a sphere to the scene
void AddSphere(glm::vec3 position, float radius, glm::vec3 color, float reflectivity = 0.5f) {
    if (numObjects < MAX_OBJECTS) {
        sceneObjects[numObjects].type = 0; // Sphere
        sceneObjects[numObjects].position = position;
        sceneObjects[numObjects].size = glm::vec3(radius);
        sceneObjects[numObjects].color = color;
        sceneObjects[numObjects].reflectivity = reflectivity;
        numObjects++;
    }
}

// Function to add a cube to the scene
void AddCube(glm::vec3 position, glm::vec3 size, glm::vec3 color, float reflectivity = 0.5f) {
    if (numObjects < MAX_OBJECTS) {
        sceneObjects[numObjects].type = 1; // Cube
        sceneObjects[numObjects].position = position;
        sceneObjects[numObjects].size = size;
        sceneObjects[numObjects].color = color;
        sceneObjects[numObjects].reflectivity = reflectivity;
        numObjects++;
    }
}

// Function to add a light to the scene
void AddLight(glm::vec3 position, glm::vec3 color, float intensity) {
    if (numLights < MAX_LIGHTS) {
        lights[numLights].position = position;
        lights[numLights].color = color;
        lights[numLights].intensity = intensity;
        numLights++;
    }
}

// Function to set up a basic scene
void SetupScene() {
    // Clear any existing objects
    numObjects = 0;
    numLights = 0;
    
    // Add objects to the scene
    AddSphere(glm::vec3(0.0f, 0.0f, 0.0f), 0.5f, glm::vec3(1.0f, 0.2f, 0.2f), 0.7f);
    AddSphere(glm::vec3(1.0f, 0.0f, 1.0f), 0.3f, glm::vec3(0.2f, 0.8f, 0.2f), 0.9f);
    AddCube(glm::vec3(-1.0f, -0.5f, 0.0f), glm::vec3(0.5f), glm::vec3(0.2f, 0.2f, 1.0f), 0.3f);
    AddCube(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(5.0f, 0.1f, 5.0f), glm::vec3(0.8f, 0.8f, 0.8f), 0.2f);
    
    // Add lights
    AddLight(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
    AddLight(glm::vec3(-5.0f, 3.0f, -3.0f), glm::vec3(0.5f, 0.5f, 0.8f), 0.8f);
}

// Function to create a fullscreen quad for ray tracing
void CreateQuad() {
    float quadVertices[] = {
        // positions        // texture coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    
    // setup plane VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

// Function to compile the ray tracing shader
GLuint CompileRayTraceShaders() {
    GLuint rayTraceProgramID = glCreateProgram();
    
    if (rayTraceProgramID == 0) {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }
    
    std::string vs, fs;
    
    if (!ReadFile(pRayTraceVSFileName, vs)) {
        fprintf(stderr, "Error reading vertex shader for ray tracing\n");
        exit(1);
    }
    
    if (!ReadFile(pRayTraceFSFileName, fs)) {
        fprintf(stderr, "Error reading fragment shader for ray tracing\n");
        exit(1);
    }
    
    AddShader(rayTraceProgramID, vs.c_str(), GL_VERTEX_SHADER);
    AddShader(rayTraceProgramID, fs.c_str(), GL_FRAGMENT_SHADER);
    
    GLint Success = 0;
    GLchar ErrorLog[1024] = {0};
    
    glLinkProgram(rayTraceProgramID);
    glGetProgramiv(rayTraceProgramID, GL_LINK_STATUS, &Success);
    if (Success == 0) {
        glGetProgramInfoLog(rayTraceProgramID, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Error linking ray tracing shader program: '%s'\n", ErrorLog);
        exit(1);
    }
    
    return rayTraceProgramID;
}

// Create shaders and quad for ray tracing
void InitRayTracing() {
    // Compile the ray tracing shader
    rayTraceProgramID = CompileRayTraceShaders();
    
    // Create the quad for ray tracing
    CreateQuad();
    
    // Setup the initial scene
    SetupScene();
}

/********************************************************************
  Utility functions
 */

/* post: compute frames per second and display in window's title bar */
void computeFPS()
{
	static int frameCount = 0;
	static int lastFrameTime = 0;
	static char *title = NULL;
	int currentTime;

	if (!title)
		title = (char *)malloc((strlen(theProgramTitle) + 20) * sizeof(char));
	frameCount++;
	currentTime = 0;
	if (currentTime - lastFrameTime > 1000)
	{
		snprintf(title, strlen(theProgramTitle) + 20,"%s [ FPS: %4.2f ]",
				theProgramTitle,
				frameCount * 1000.0 / (currentTime - lastFrameTime));
		lastFrameTime = currentTime;
		frameCount = 0;
	}
}

static void LoadOffModel() {
    model = readOffFile(offFilePath);
    
    if (model == NULL) {
        fprintf(stderr, "Failed to load OFF model: %s\n", offFilePath);
        exit(1);
    }
    
    printf("Loaded model with %d vertices and %d polygons\n", 
           model->numberOfVertices, model->numberOfPolygons);
    
    // Calculate model center for normalization
    float centerX = (model->minX + model->maxX) / 2.0f;
    float centerY = (model->minY + model->maxY) / 2.0f;
    float centerZ = (model->minZ + model->maxZ) / 2.0f;
    
    // For normalization
    float scale = 2.0f / model->extent;
    
    // Count total number of indices needed
    numIndices = 0;
    for (int i = 0; i < model->numberOfPolygons; i++) {
        // We'll convert polygons to triangles
        if (model->polygons[i].noSides >= 3) {
            numIndices += (model->polygons[i].noSides - 2) * 3;
        }
    }
    
    // Prepare vertex array and index array
    Vector3f* vertices = new Vector3f[model->numberOfVertices];
    unsigned int* indices = new unsigned int[numIndices];
    
    // Copy vertices with normalization
    for (int i = 0; i < model->numberOfVertices; i++) {
        vertices[i].x = (model->vertices[i].x - centerX) * scale;
        vertices[i].y = (model->vertices[i].y - centerY) * scale;
        vertices[i].z = (model->vertices[i].z - centerZ) * scale;
    }
    
    // Convert polygons to triangles using triangle fan method
    int indexCount = 0;
    for (int i = 0; i < model->numberOfPolygons; i++) {
        Polygon poly = model->polygons[i];
        
        // Skip degenerate polygons
        if (poly.noSides < 3) continue;
        
        // Triangle fan triangulation
        for (int j = 0; j < poly.noSides - 2; j++) {
            indices[indexCount++] = poly.v[0];         // First vertex
            indices[indexCount++] = poly.v[j + 1];     // Second vertex
            indices[indexCount++] = poly.v[j + 2];     // Third vertex
        }
    }
    
    // Create and bind a vertex array object
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Create and populate the vertex buffer object
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model->numberOfVertices * sizeof(Vector3f), vertices, GL_STATIC_DRAW);
    
    // Create and populate the index buffer object
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3f), 0);
    
    // Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    delete[] vertices;
    delete[] indices;
    
    numVertices = model->numberOfVertices;
}

static void CreateVertexBuffer() {
    LoadOffModel();
}

static void AddShader(GLuint ShaderProgram, const char *pShaderText, GLenum ShaderType)
{
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0)
	{
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
		exit(0);
	}

	const GLchar *p[1];
	p[0] = pShaderText;
	GLint Lengths[1];
	Lengths[0] = strlen(pShaderText);
	glShaderSource(ShaderObj, 1, p, Lengths);
	glCompileShader(ShaderObj);
	GLint success;
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		exit(1);
	}

	glAttachShader(ShaderProgram, ShaderObj);
}

using namespace std;

static void CompileShaders()
{
	GLuint ShaderProgram = glCreateProgram();

	if (ShaderProgram == 0)
	{
		fprintf(stderr, "Error creating shader program\n");
		exit(1);
	}

	string vs, fs;

	if (!ReadFile(pVSFileName, vs))
	{
		exit(1);
	}

	if (!ReadFile(pFSFileName, fs))
	{
		exit(1);
	}

	AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = {0};

	glLinkProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
	if (Success == 0)
	{
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
		exit(1);
	}
	glBindVertexArray(VAO);
	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success)
	{
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Invalid shader program1: '%s'\n", ErrorLog);
		exit(1);
	}

	glUseProgram(ShaderProgram);
	gWorldLocation = glGetUniformLocation(ShaderProgram, "gWorld");
	gViewLocation = glGetUniformLocation(ShaderProgram, "gView");
	gProjectionLocation = glGetUniformLocation(ShaderProgram, "gProjection");
}

/********************************************************************
 Callback Functions
 */

void onInit(int argc, char *argv[])
{
	/* by default the back ground color is black */
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	CreateVertexBuffer();
	CompileShaders();
    InitRayTracing();

	/* set to draw in window based on depth  */
	glEnable(GL_DEPTH_TEST);
}

void RenderRayTracing() {
    glUseProgram(rayTraceProgramID);
    
    // Set uniform variables for the ray tracer
    
    // Camera parameters
    GLint cameraPositionLoc = glGetUniformLocation(rayTraceProgramID, "cameraPosition");
    glUniform3fv(cameraPositionLoc, 1, glm::value_ptr(cameraPosition));
    
    // View and projection matrices
    glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
    GLint viewMatrixLoc = glGetUniformLocation(rayTraceProgramID, "viewMatrix");
    glUniformMatrix4fv(viewMatrixLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    
    // We need the projection matrix for proper ray direction calculation
    int width, height;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    float aspectRatio = (float)width / (float)height;
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(cameraFOV), aspectRatio, 0.1f, 100.0f);
    GLint projectionMatrixLoc = glGetUniformLocation(rayTraceProgramID, "projectionMatrix");
    glUniformMatrix4fv(projectionMatrixLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    
    // Screen dimensions
    GLint screenWidthLoc = glGetUniformLocation(rayTraceProgramID, "screenWidth");
    GLint screenHeightLoc = glGetUniformLocation(rayTraceProgramID, "screenHeight");
    glUniform1f(screenWidthLoc, (float)width);
    glUniform1f(screenHeightLoc, (float)height);
    
    // Ray tracing settings
    GLint enableShadowsLoc = glGetUniformLocation(rayTraceProgramID, "enableShadows");
    GLint enableReflectionsLoc = glGetUniformLocation(rayTraceProgramID, "enableReflections");
    GLint maxBouncesLoc = glGetUniformLocation(rayTraceProgramID, "maxBounces");
    GLint reflectivityLoc = glGetUniformLocation(rayTraceProgramID, "reflectivity");
    glUniform1i(enableShadowsLoc, enableShadows ? 1 : 0);
    glUniform1i(enableReflectionsLoc, enableReflections ? 1 : 0);
    glUniform1i(maxBouncesLoc, maxBounces);
    glUniform1f(reflectivityLoc, reflectivity);
    
    // Set scene objects
    GLint numObjectsLoc = glGetUniformLocation(rayTraceProgramID, "numObjects");
    glUniform1i(numObjectsLoc, numObjects);
    
    for (int i = 0; i < numObjects; i++) {
        char buffer[64];
        
        snprintf(buffer, sizeof(buffer), "objects[%d].type", i);
        GLint typeLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "objects[%d].position", i);
        GLint posLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "objects[%d].size", i);
        GLint sizeLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "objects[%d].color", i);
        GLint colorLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "objects[%d].reflectivity", i);
        GLint reflLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        glUniform1i(typeLoc, sceneObjects[i].type);
        glUniform3fv(posLoc, 1, glm::value_ptr(sceneObjects[i].position));
        glUniform3fv(sizeLoc, 1, glm::value_ptr(sceneObjects[i].size));
        glUniform3fv(colorLoc, 1, glm::value_ptr(sceneObjects[i].color));
        glUniform1f(reflLoc, sceneObjects[i].reflectivity);
    }
    
    // Set lights
    GLint numLightsLoc = glGetUniformLocation(rayTraceProgramID, "numLights");
    glUniform1i(numLightsLoc, numLights);
    
    for (int i = 0; i < numLights; i++) {
        char buffer[64];
        
        snprintf(buffer, sizeof(buffer), "lights[%d].position", i);
        GLint posLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "lights[%d].color", i);
        GLint colorLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        snprintf(buffer, sizeof(buffer), "lights[%d].intensity", i);
        GLint intensityLoc = glGetUniformLocation(rayTraceProgramID, buffer);
        
        glUniform3fv(posLoc, 1, glm::value_ptr(lights[i].position));
        glUniform3fv(colorLoc, 1, glm::value_ptr(lights[i].color));
        glUniform1f(intensityLoc, lights[i].intensity);
    }
    
    // Set ambient light
    GLint ambientLightLoc = glGetUniformLocation(rayTraceProgramID, "ambientLight");
    glUniform3fv(ambientLightLoc, 1, glm::value_ptr(ambientLight));
    
    // Render the quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

static void onDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (useRayTracing) {
        RenderRayTracing();
    } else {
        // Create rotation matrix using GLM
        glm::mat4 rotationMatrix = glm::rotate(
            glm::mat4(1.0f),  // Identity matrix
            rotation,         // Rotation angle in radians
            glm::vec3(0.0f, 1.0f, 0.0f)  // Rotate around Y-axis
        );
        
        // Model matrix// 
        glm::mat4 worldMatrix = glm::mat4(1.0f); // Identity matrix
        
        // View matrix - camera at (0,0,10) looking at origin (0,0,0)
        glm::mat4 viewMatrix = glm::lookAt(
            cameraPosition, // Camera position
            cameraTarget,   // Look at point
            cameraUp        // Up vector
        );
        
        // Get current window size to ensure correct aspect ratio
        int width, height;
        glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
        float aspectRatio = (float)width / (float)height;
        
        // Projection matrix - 45° FOV with proper aspect ratio
        glm::mat4 projectionMatrix = glm::perspective(
            glm::radians(cameraFOV),     // FOV
            aspectRatio,                 // Aspect ratio using actual window dimensions
            0.1f,                        // Near clipping plane
            100.0f                       // Far clipping plane
        );
        
        // Pass all matrices to the shader
        glUseProgram(gWorldLocation);
        glUniformMatrix4fv(gWorldLocation, 1, GL_FALSE, glm::value_ptr(worldMatrix));
        glUniformMatrix4fv(gViewLocation, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(gProjectionLocation, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
        
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

	/* check for any errors when rendering */
	GLenum errorCode = glGetError();
	if (errorCode == GL_NO_ERROR)
	{
	}
	else
	{
		fprintf(stderr, "OpenGL rendering error %d\n", errorCode);
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	// std::cout << "Mouse position: " << xpos << ", " << ypos << std::endl;
    // Handle mouse movement
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Left button pressed
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Left button released
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)  // Only react on key press
    {
        switch (key)
        {
        case GLFW_KEY_R:
            rotation = 0;
            break;
        case GLFW_KEY_Q:
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, true);
            break;
        }
    }
}

// Initialize ImGui
void InitImGui(GLFWwindow *window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

// Render ImGui with ray tracing options
void RenderImGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	static float rotationSpeed = 0.01f;
	static bool autoRotate = true;
	static int rotationAxis = 1; // 0 = X, 1 = Y, 2 = Z
	static float fieldOfView = 45.0f;  // Add FOV control

	ImGui::Begin("Ray Tracing Control Panel");
	
    // Rendering mode
    ImGui::Checkbox("Use Ray Tracing", &useRayTracing);
    
    if (useRayTracing) {
        // Ray tracing settings
        if (ImGui::CollapsingHeader("Ray Tracing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable Shadows", &enableShadows);
            ImGui::Checkbox("Enable Reflections", &enableReflections);
            ImGui::SliderInt("Max Reflection Bounces", &maxBounces, 0, 10);
            ImGui::SliderFloat("Global Reflectivity", &reflectivity, 0.0f, 1.0f);
        }
        
        // Camera settings
        if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Field of View", &cameraFOV, 30.0f, 90.0f);
            ImGui::Text("Camera Position");
            ImGui::SliderFloat("Camera X", &cameraPosition.x, -20.0f, 20.0f);
            ImGui::SliderFloat("Camera Y", &cameraPosition.y, -20.0f, 20.0f);
            ImGui::SliderFloat("Camera Z", &cameraPosition.z, -20.0f, 20.0f);
        }
        
        // Scene objects
        if (ImGui::CollapsingHeader("Scene Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Objects: %d", numObjects);
            
            for (int i = 0; i < numObjects; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Object %d", i);
                
                if (ImGui::TreeNode(label)) {
                    const char* typeNames[] = { "Sphere", "Cube", "Mesh" };
                    ImGui::Text("Type: %s", typeNames[sceneObjects[i].type]);
                    
                    ImGui::Text("Position");
                    ImGui::SliderFloat("X##pos", &sceneObjects[i].position.x, -5.0f, 5.0f);
                    ImGui::SliderFloat("Y##pos", &sceneObjects[i].position.y, -5.0f, 5.0f);
                    ImGui::SliderFloat("Z##pos", &sceneObjects[i].position.z, -5.0f, 5.0f);
                    
                    if (sceneObjects[i].type == 0) { // Sphere
                        ImGui::SliderFloat("Radius", &sceneObjects[i].size.x, 0.1f, 3.0f);
                    } else if (sceneObjects[i].type == 1) { // Cube
                        ImGui::Text("Size");
                        ImGui::SliderFloat("X##size", &sceneObjects[i].size.x, 0.1f, 5.0f);
                        ImGui::SliderFloat("Y##size", &sceneObjects[i].size.y, 0.1f, 5.0f);
                        ImGui::SliderFloat("Z##size", &sceneObjects[i].size.z, 0.1f, 5.0f);
                    }
                    
                    ImGui::Text("Color");
                    ImGui::ColorEdit3("##color", glm::value_ptr(sceneObjects[i].color));
                    ImGui::SliderFloat("Reflectivity", &sceneObjects[i].reflectivity, 0.0f, 1.0f);
                    
                    ImGui::TreePop();
                }
            }
            
            if (ImGui::Button("Reset Scene")) {
                SetupScene();
            }
        }
        
        // Lights
        if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Lights: %d", numLights);
            
            // Ambient light
            ImGui::Text("Ambient Light");
            ImGui::ColorEdit3("##ambient", glm::value_ptr(ambientLight));
            
            // Point lights
            for (int i = 0; i < numLights; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Light %d", i);
                
                if (ImGui::TreeNode(label)) {
                    ImGui::Text("Position");
                    ImGui::SliderFloat("X##lightpos", &lights[i].position.x, -20.0f, 20.0f);
                    ImGui::SliderFloat("Y##lightpos", &lights[i].position.y, -20.0f, 20.0f);
                    ImGui::SliderFloat("Z##lightpos", &lights[i].position.z, -20.0f, 20.0f);
                    
                    ImGui::Text("Color");
                    ImGui::ColorEdit3("##lightcolor", glm::value_ptr(lights[i].color));
                    
                    ImGui::SliderFloat("Intensity", &lights[i].intensity, 0.0f, 5.0f);
                    
                    ImGui::TreePop();
                }
            }
        }
    } else {
        // Standard rendering settings
        if (model) {
            ImGui::Text("Model: %s", offFilePath);
            ImGui::Text("Vertices: %d", model->numberOfVertices);
            ImGui::Text("Polygons: %d", model->numberOfPolygons);
            ImGui::Text("Total Triangles: %d", numIndices / 3);
            
            ImGui::Separator();
            ImGui::Text("Camera Controls:");
            ImGui::SliderFloat("Field of View", &cameraFOV, 30.0f, 90.0f);
            
            ImGui::Separator();
            ImGui::Text("Rotation Controls:");
            ImGui::SliderFloat("Rotation", &rotation, 0.0f, 6.28f);
            ImGui::Checkbox("Auto Rotate", &autoRotate);
            if (autoRotate) {
                ImGui::SliderFloat("Speed", &rotationSpeed, 0.001f, 0.05f);
                
                // Update rotation if auto-rotate is enabled
                rotation += rotationSpeed;
                if (rotation > 6.28f) rotation -= 6.28f;
            }
            
            ImGui::Text("Rotation Axis");
            ImGui::RadioButton("X-Axis", &rotationAxis, 0); ImGui::SameLine();
            ImGui::RadioButton("Y-Axis", &rotationAxis, 1); ImGui::SameLine();
            ImGui::RadioButton("Z-Axis", &rotationAxis, 2);
        } else {
            ImGui::Text("No model loaded");
        }
    }
	
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Define main function
int main(int argc, char *argv[])
{
	// Initialize GLFW
	glfwInit();

	// Define version and compatibility settings
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);  // Allow resizing for better aspect ratio control

	// Create OpenGL window and context
	GLFWwindow *window = glfwCreateWindow(800, 800, "OpenGL", NULL, NULL);  // Use square window dimensions
	glfwMakeContextCurrent(window);

	// Check for window creation failure
	if (!window)
	{
		// Terminate GLFW
		glfwTerminate();
		return 0;
	}

	// Initialize GLEW
	glewExperimental = GL_TRUE;
	glewInit();
	printf("GL version: %s\n", glGetString(GL_VERSION));
	onInit(argc, argv);

	// Initialize ImGui
	InitImGui(window);

	// Set GLFW callback functions
	glfwSetKeyCallback(window, key_callback);

	// Event loop
	while (!glfwWindowShouldClose(window))
	{
		// Clear the screen to black
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		onDisplay();

		RenderImGui();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Clean up OpenGL resources
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &IBO);

	// Free the model
	if (model) {
		FreeOffModel(model);
	}

	// Clean up ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	// Terminate GLFW
	glfwTerminate();
	return 0;
}
