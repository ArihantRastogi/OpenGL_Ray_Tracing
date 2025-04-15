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

char theProgramTitle[] = "Sample";
int theWindowWidth = 700, theWindowHeight = 700;
int theWindowPositionX = 40, theWindowPositionY = 40;
bool isFullScreen = false;
bool isAnimating = true;
float rotation = 0.0f;
GLuint VBO, VAO, IBO;
GLuint gWorldLocation;
GLuint gViewLocation;
GLuint gProjectionLocation;

OffModel* model = NULL;
int numVertices = 0;
int numIndices = 0;

/* Constants */
const int ANIMATION_DELAY = 20; /* milliseconds between rendering */
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";
char* offFilePath = "models/cube.off";


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
		sprintf(title, "%s [ FPS: %4.2f ]",
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

	/* set to draw in window based on depth  */
	glEnable(GL_DEPTH_TEST);
}

static void onDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Create rotation matrix using GLM
	glm::mat4 rotationMatrix = glm::rotate(
		glm::mat4(1.0f),  // Identity matrix
		rotation,         // Rotation angle in radians
		glm::vec3(0.0f, 1.0f, 0.0f)  // Rotate around Y-axis
	);
	
	// Model matrix// 
	glm::mat4 worldMatrix = rotationMatrix;
	
	// View matrix - camera at (0,0,10) looking at origin (0,0,0)
	glm::mat4 viewMatrix = glm::lookAt(
		glm::vec3(0.0f, 0.0f, 10.0f), // Camera position
		glm::vec3(0.0f, 0.0f, 0.0f),  // Look at point
		glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
	);
	
	// Get current window size to ensure correct aspect ratio
	int width, height;
	glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
	float aspectRatio = (float)width / (float)height;
	
	// Projection matrix - 45° FOV with proper aspect ratio
	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(45.0f),            // FOV
		aspectRatio,                    // Aspect ratio using actual window dimensions
		0.1f,                           // Near clipping plane
		100.0f                          // Far clipping plane
	);
	
	// Pass all matrices to the shader
	glUniformMatrix4fv(gWorldLocation, 1, GL_FALSE, glm::value_ptr(worldMatrix));
	glUniformMatrix4fv(gViewLocation, 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(gProjectionLocation, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

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

// Render ImGui
void RenderImGui()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	static float rotationSpeed = 0.01f;
	static bool autoRotate = true;
	static int rotationAxis = 1; // 0 = X, 1 = Y, 2 = Z
	static float cameraZ = 10.0f;
	static float fieldOfView = 45.0f;  // Add FOV control

	ImGui::Begin("Model Information");
	
	if (model) {
		ImGui::Text("Model: %s", offFilePath);
		ImGui::Text("Vertices: %d", model->numberOfVertices);
		ImGui::Text("Polygons: %d", model->numberOfPolygons);
		ImGui::Text("Total Triangles: %d", numIndices / 3);
		
		ImGui::Separator();
		ImGui::Text("Camera Controls:");
		ImGui::SliderFloat("Camera Z", &cameraZ, 3.0f, 20.0f);
		ImGui::SliderFloat("Field of View", &fieldOfView, 30.0f, 90.0f);  // Allow FOV adjustment
		
		ImGui::Separator();
		ImGui::Text("Rotation Controls:");
		ImGui::SliderFloat("Rotation", &rotation, 0.0f, 6.28f);
		ImGui::Checkbox("Auto Rotate", &autoRotate);
		if (autoRotate) {
			ImGui::SliderFloat("Speed", &rotationSpeed, 0.001f, 0.05f);
		}
		
		ImGui::Text("Rotation Axis");
		ImGui::RadioButton("X-Axis", &rotationAxis, 0); ImGui::SameLine();
		ImGui::RadioButton("Y-Axis", &rotationAxis, 1); ImGui::SameLine();
		ImGui::RadioButton("Z-Axis", &rotationAxis, 2);

		ImGui::Separator();
		ImGui::Text("Model Bounds:");
		ImGui::Text("X: %.2f to %.2f", model->minX, model->maxX);
		ImGui::Text("Y: %.2f to %.2f", model->minY, model->maxY);
		ImGui::Text("Z: %.2f to %.2f", model->minZ, model->maxZ);
		
		// Update rotation if auto-rotate is enabled
		if (autoRotate) {
			rotation += rotationSpeed;
			if (rotation > 6.28f) rotation -= 6.28f;
		}
	} else {
		ImGui::Text("No model loaded");
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
