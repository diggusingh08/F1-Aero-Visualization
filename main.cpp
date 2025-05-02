#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Model.h"
#include "FlowVisualization.h"

#include <iostream>
#include <cstdlib>
#include <direct.h>
#include <windows.h>

// Camera variables - adjusted for side view
glm::vec3 cameraPos = glm::vec3(5.0f, 1.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(-1.0f, 0.0f, 0.0f);
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
bool showFlow = true;
bool showCar = true;

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);
void printCurrentDirectory();

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
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Scroll callback function for zoom
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

// Key callback for special actions
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        showFlow = !showFlow;
        std::cout << "Flow visualization: " << (showFlow ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        showCar = !showCar;
        std::cout << "Car visibility: " << (showCar ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 1.0f, 5.0f);
        yaw = -90.0f;
        pitch = 0.0f;
        cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        std::cout << "Camera reset" << std::endl;
    }
}

// Process keyboard input for camera movement
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 5.0f * deltaTime;
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
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

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
    GLFWwindow* window = glfwCreateWindow(1200, 800, "F1 Car Aero Visualization", NULL, NULL);
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
        std::cout << "Loading shaders..." << std::endl;

        // Main model shader
        Shader ourShader("vertex.glsl", "fragment.glsl");
        std::cout << "Car shader loaded successfully!" << std::endl;

        // Line shader for flow visualization
        Shader lineShader("line_vertex.glsl", "line_fragment.glsl");
        std::cout << "Line shader loaded successfully!" << std::endl;

        // 7. Load model
        std::string modelPath = "C:/Users/hp/Desktop/C assgn/ComputerGraphicsProject/F1_Project_lib/F1_Project_lib/x64/Release/mcl35m_2.obj";
        std::cout << "Loading model from: " << modelPath << std::endl;
        Model ourModel(modelPath);
        std::cout << "Model loaded successfully!" << std::endl;

        // 8. Create flow lines visualization
        float carLength = 5.7f;
        float carWidth = 2.0f;
        float carHeight = 1.0f;
        int numLines = 500; // Number of flow lines
        FlowLinesVisualization flowLinesVis(numLines, carLength, carWidth, carHeight);
        std::cout << "Flow lines visualization initialized with " << numLines << " lines!" << std::endl;

        // 9. OpenGL settings
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // 10. Main loop
        while (!glfwWindowShouldClose(window)) {
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(fov), 1200.0f / 800.0f, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            if (showCar) {
                ourShader.use();

                ourShader.setVec3("lightPos", glm::vec3(5.0f, 5.0f, 5.0f));
                ourShader.setVec3("viewPos", cameraPos);
                ourShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
                ourShader.setVec3("objectColor", glm::vec3(0.8f, 0.1f, 0.1f));

                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                float scale = 1.0f;
                model = glm::scale(model, glm::vec3(scale));

                ourShader.setMat4("model", model);
                ourModel.Draw(ourShader);
            }

            if (showFlow) {
                flowLinesVis.update(deltaTime);
                flowLinesVis.draw(lineShader, view, projection);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        flowLinesVis.cleanup();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}