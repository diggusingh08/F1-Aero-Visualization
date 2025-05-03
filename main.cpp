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
#include <chrono>
#include <thread>

// Camera variables - adjusted for side view by default
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
bool usePressureMap = true;
bool simulateDRS = false;
int flowDensity = 350; // Reduced from 400 to avoid congestion
float streamlineDensity = 0.20f; // Controls spacing between streamlines
bool enableAdaptiveDensity = true;

// Simulation variables
float carSpeed = 250.0f; // km/h - affects flow behavior
bool pauseSimulation = false;

// Car movement variables
float carPosition = 0.0f; // Z position of the car
float movementSpeed = 0.0f; // Current movement speed
float maxMovementSpeed = 10.0f; // Maximum movement speed
bool carMoving = false; // Is the car currently moving?
bool cameraMoveWithCar = false; // Should camera follow the car?

// Rendering variables
int windowWidth = 1200;
int windowHeight = 800;

// Camera presets
struct CameraPreset {
    glm::vec3 position;
    float presetYaw;
    float presetPitch;
    std::string name;
};

std::vector<CameraPreset> cameraPresets = {
    {{0.0f, 1.5f, -8.0f}, 90.0f, 0.0f, "Front View"},
    {{5.0f, 1.5f, 0.0f}, 180.0f, 0.0f, "Side View"},
    {{0.0f, 5.0f, 0.0f}, -90.0f, -89.0f, "Top View"},
    {{5.0f, 3.0f, 5.0f}, 225.0f, -30.0f, "3/4 View"}
};

int currentPreset = 0;

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);
void printCurrentDirectory();
void setCurrentCameraPreset();
void printSimulationInfo();
void updateCarMovement(float deltaTime);

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
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        usePressureMap = !usePressureMap;
        std::cout << "Pressure map: " << (usePressureMap ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        simulateDRS = !simulateDRS;
        std::cout << "DRS simulation: " << (simulateDRS ? "OPEN" : "CLOSED") << std::endl;
    }
    if (key == GLFW_KEY_V && action == GLFW_PRESS) {
        currentPreset = (currentPreset + 1) % cameraPresets.size();
        setCurrentCameraPreset();
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        pauseSimulation = !pauseSimulation;
        std::cout << "Simulation: " << (pauseSimulation ? "PAUSED" : "RUNNING") << std::endl;
    }
    if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
        carSpeed += 10.0f;
        if (carSpeed > 350.0f) carSpeed = 350.0f;
        std::cout << "Car speed: " << carSpeed << " km/h" << std::endl;
    }
    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        carSpeed -= 10.0f;
        if (carSpeed < 0.0f) carSpeed = 0.0f;
        std::cout << "Car speed: " << carSpeed << " km/h" << std::endl;
    }
    if (key == GLFW_KEY_KP_ADD && action == GLFW_PRESS || key == GLFW_KEY_EQUAL && action == GLFW_PRESS) {
        streamlineDensity -= 0.05f;
        if (streamlineDensity < 0.1f) streamlineDensity = 0.1f;
        std::cout << "Streamline density increased" << std::endl;
    }
    if (key == GLFW_KEY_KP_SUBTRACT && action == GLFW_PRESS || key == GLFW_KEY_MINUS && action == GLFW_PRESS) {
        streamlineDensity += 0.05f;
        if (streamlineDensity > 1.0f) streamlineDensity = 1.0f;
        std::cout << "Streamline density decreased" << std::endl;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        enableAdaptiveDensity = !enableAdaptiveDensity;
        std::cout << "Adaptive density: " << (enableAdaptiveDensity ? "ON" : "OFF") << std::endl;
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        // Reset camera to current preset
        setCurrentCameraPreset();
        std::cout << "Camera reset to " << cameraPresets[currentPreset].name << std::endl;
    }
    if (key == GLFW_KEY_I && action == GLFW_PRESS) {
        printSimulationInfo();
    }

    // Car movement controls
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        carMoving = !carMoving;
        std::cout << "Car movement: " << (carMoving ? "ON" : "OFF") << std::endl;

        // Reset movement speed if stopping
        if (!carMoving) {
            movementSpeed = 0.0f;
        }
        else {
            // Start moving forward if not already moving
            if (movementSpeed == 0.0f) {
                movementSpeed = maxMovementSpeed * 0.3f;
            }
        }
    }

    // Forward/backward movement
    if (key == GLFW_KEY_J && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        // Backward movement
        if (movementSpeed > -maxMovementSpeed) {
            movementSpeed -= 0.5f;
        }
        carMoving = true;
        std::cout << "Car movement speed: " << movementSpeed << std::endl;
    }
    if (key == GLFW_KEY_K && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        // Forward movement
        if (movementSpeed < maxMovementSpeed) {
            movementSpeed += 0.5f;
        }
        carMoving = true;
        std::cout << "Car movement speed: " << movementSpeed << std::endl;
    }
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        // Stop movement
        movementSpeed = 0.0f;
        carMoving = false;
        std::cout << "Car stopped" << std::endl;
    }

    // Toggle camera following car
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        cameraMoveWithCar = !cameraMoveWithCar;
        std::cout << "Camera follows car: " << (cameraMoveWithCar ? "ON" : "OFF") << std::endl;
    }
}

void setCurrentCameraPreset() {
    cameraPos = cameraPresets[currentPreset].position;
    yaw = cameraPresets[currentPreset].presetYaw;
    pitch = cameraPresets[currentPreset].presetPitch;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    std::cout << "Camera preset: " << cameraPresets[currentPreset].name << std::endl;
}

void printSimulationInfo() {
    std::cout << "\n--- SIMULATION INFORMATION ---" << std::endl;
    std::cout << "Car speed: " << carSpeed << " km/h" << std::endl;
    std::cout << "Car movement: " << (carMoving ? "ON" : "OFF") << std::endl;
    std::cout << "Movement speed: " << movementSpeed << std::endl;
    std::cout << "Car position: " << carPosition << std::endl;
    std::cout << "Camera follows car: " << (cameraMoveWithCar ? "ON" : "OFF") << std::endl;
    std::cout << "Flow visualization: " << (showFlow ? "ON" : "OFF") << std::endl;
    std::cout << "Streamline density: " << (1.0f / streamlineDensity) << std::endl;
    std::cout << "Adaptive density: " << (enableAdaptiveDensity ? "ON" : "OFF") << std::endl;
    std::cout << "Pressure mapping: " << (usePressureMap ? "ON" : "OFF") << std::endl;
    std::cout << "DRS: " << (simulateDRS ? "OPEN" : "CLOSED") << std::endl;
    std::cout << "Camera: " << cameraPresets[currentPreset].name << std::endl;
    std::cout << "Simulation: " << (pauseSimulation ? "PAUSED" : "RUNNING") << std::endl;
    std::cout << "-----------------------------\n" << std::endl;
}

// Update car movement
void updateCarMovement(float deltaTime) {
    if (carMoving && !pauseSimulation) {
        // Update car position based on movement speed
        carPosition += movementSpeed * deltaTime;

        // Update camera position if following car
        if (cameraMoveWithCar) {
            // Offset camera position by car position in Z-axis
            glm::vec3 originalPos = cameraPresets[currentPreset].position;
            cameraPos.z = originalPos.z + carPosition;
        }
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
    windowWidth = width;
    windowHeight = height;
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
    glfwWindowHint(GLFW_SAMPLES, 4); // Enable MSAA

    // 3. Create Window
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "F1 Car Aero Visualization", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    // Set initial camera position to first preset
    setCurrentCameraPreset();

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

    // 6. Print simulation controls
    std::cout << "\n--- F1 AERODYNAMICS VISUALIZATION ---" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD/QE: Move camera" << std::endl;
    std::cout << "  Mouse: Look around" << std::endl;
    std::cout << "  Scroll: Zoom" << std::endl;
    std::cout << "  F: Toggle flow visualization" << std::endl;
    std::cout << "  C: Toggle car visibility" << std::endl;
    std::cout << "  P: Toggle pressure/velocity map" << std::endl;
    std::cout << "  D: Toggle DRS (open/closed)" << std::endl;
    std::cout << "  V: Cycle camera views" << std::endl;
    std::cout << "  R: Reset camera to preset" << std::endl;
    std::cout << "  SPACE: Pause/resume simulation" << std::endl;
    std::cout << "  UP/DOWN: Increase/decrease car speed" << std::endl;
    std::cout << "  +/-: Increase/decrease flow density" << std::endl;
    std::cout << "  A: Toggle adaptive density" << std::endl;
    std::cout << "  I: Show simulation information" << std::endl;
    std::cout << "  M: Toggle car movement" << std::endl;
    std::cout << "  J/K: Decrease/increase movement speed" << std::endl;
    std::cout << "  L: Stop car movement" << std::endl;
    std::cout << "  T: Toggle camera following car" << std::endl;
    std::cout << "  ESC: Exit" << std::endl;
    std::cout << "-------------------------------------\n" << std::endl;

    // 7. Load shaders and model
    try {
        std::cout << "Loading shaders..." << std::endl;

        // Main model shader
        Shader ourShader("vertex.glsl", "fragment.glsl");
        std::cout << "Car shader loaded successfully!" << std::endl;

        // Line shader for flow visualization
        Shader lineShader("line_vertex.glsl", "line_fragment.glsl");
        std::cout << "Line shader loaded successfully!" << std::endl;

        // 8. Load model
        std::string modelPath = "C:/Users/hp/Desktop/C assgn/ComputerGraphicsProject/F1_Project_lib/F1_Project_lib/x64/Release/mcl35m_2.obj";
        std::cout << "Loading model from: " << modelPath << std::endl;
        Model ourModel(modelPath);
        std::cout << "Model loaded successfully!" << std::endl;

        // 9. Create flow lines visualization
        float carLength = 5.7f;
        float carWidth = 2.0f;
        float carHeight = 1.0f;
        FlowLinesVisualization flowLinesVis(flowDensity, carLength, carWidth, carHeight);
        flowLinesVis.setDensity(streamlineDensity);
        flowLinesVis.setAdaptiveDensity(enableAdaptiveDensity);
        std::cout << "Flow lines visualization initialized with " << flowDensity << " lines!" << std::endl;

        // 10. OpenGL settings
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_MULTISAMPLE); // Enable MSAA

        // Print initial simulation info
        printSimulationInfo();

        // 11. Main loop
        while (!glfwWindowShouldClose(window)) {
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            // Limit frame rate to avoid too fast simulation on powerful GPUs
            if (deltaTime < 0.01f) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10 - static_cast<int>(deltaTime * 1000)));
                currentFrame = glfwGetTime();
                deltaTime = currentFrame - lastFrame;
                lastFrame = currentFrame;
            }

            processInput(window);

            // Update car movement
            updateCarMovement(deltaTime);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(glm::radians(fov), static_cast<float>(windowWidth) / windowHeight, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

            if (showCar) {
                ourShader.use();

                ourShader.setVec3("lightPos", glm::vec3(5.0f, 5.0f, 5.0f));
                ourShader.setVec3("viewPos", cameraPos);
                ourShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

                // Use McLaren orange color
                ourShader.setVec3("objectColor", glm::vec3(1.0f, 0.35f, 0.0f));

                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);

                // Center the car properly and apply position offset
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(0.0f, 0.5f, carPosition));
                // Rotate to face forward
                model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                float scale = 1.0f;
                model = glm::scale(model, glm::vec3(scale));

                ourShader.setMat4("model", model);
                ourModel.Draw(ourShader);
            }

            if (showFlow && !pauseSimulation) {
                // Update flow lines, passing car position for relative flow calculation
                flowLinesVis.update(deltaTime);
                flowLinesVis.draw(lineShader, view, projection);
            }
            else if (showFlow && pauseSimulation) {
                // If paused, just draw without updating
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