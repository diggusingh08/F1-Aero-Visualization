#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Model.h"
#include "FlowVisualization.h" // Include our new flow visualization header

#include <iostream>
#include <cstdlib>   // For getenv()
#include <direct.h>  // For _getcwd
#include <windows.h> // For Windows-specific path functions

// Camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// Mouse variables
bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;
float fov = 45.0f;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Controls
bool showFlow = true;  // Toggle flow visualization

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Mouse callback function
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; // Change this value for mouse sensitivity
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Scroll callback function for zoom
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

// Key callback for special actions
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        showFlow = !showFlow;  // Toggle flow visualization
    }
}

// Process keyboard input for camera movement
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
}

// Utility: Get current directory (Windows-compatible)
void printCurrentDirectory() {
    char buffer[FILENAME_MAX];
    if (_getcwd(buffer, FILENAME_MAX)) {
        std::cout << "Current working directory: " << buffer << std::endl;
    }
    else {
        std::cerr << "Unable to get current directory!" << std::endl;
    }
}

int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // 2. OpenGL version hint
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 3. Create Window
    GLFWwindow* window = glfwCreateWindow(800, 600, "F1 Car Aero Visualization", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    // Tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 4. Load GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // 5. Print current working directory
    printCurrentDirectory();

    // 6. Load shaders
    try {
        // Main model shader
        Shader ourShader("vertex.glsl", "fragment.glsl");

        // Particle shader for flow visualization
        Shader particleShader("particle_vertex.glsl", "particle_fragment.glsl");

        // 7. Load model
        std::string modelPath = "C:/Users/hp/Desktop/C assgn/ComputerGraphicsProject/F1_Project_lib/F1_Project_lib/x64/Release/mcl35m_2.obj";
        std::cout << "Loading model from: " << modelPath << std::endl;
        Model ourModel(modelPath);
        std::cout << "Model loaded successfully!" << std::endl;

        // 8. Create flow visualization (with approximate F1 car dimensions)
        float carLength = 5.7f;  // Approx F1 car length in meters
        float carWidth = 2.0f;   // Approx F1 car width in meters
        float carHeight = 1.0f;  // Approx F1 car height in meters
        FlowVisualization flowVis(5000, carLength, carWidth, carHeight);
        std::cout << "Flow visualization initialized!" << std::endl;

        // 9. OpenGL settings
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);  // Dark background for better visualization
        glEnable(GL_DEPTH_TEST);

        // 10. Main loop
        while (!glfwWindowShouldClose(window)) {
            // Frame time calculation
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            // Input handling
            processInput(window);

            // Render
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Setup view/projection transformations
            glm::mat4 projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            // Draw car model
            ourShader.use();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.1f)); // Scale down the car model if needed
            ourShader.setMat4("model", model);

            ourModel.Draw(ourShader);

            // Update and draw flow visualization if enabled
            if (showFlow) {
                flowVis.update(deltaTime);
                flowVis.draw(particleShader, view, projection);
            }

            // Swap buffers and poll events
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // Cleanup flow visualization
        flowVis.cleanup();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // 11. Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}