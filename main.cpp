
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.h"
#include "Camera.h"
#include "Model.h"
#include "FlowVisualization.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Globals
Camera camera(glm::vec3(0.0f, 2.0f, 10.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool carMoving = false;
bool showRearView = false;
bool showLowPressure = true;
glm::vec3 carPosition(0.0f);
float carSpeed = 5.0f;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_M:
            carMoving = !carMoving;
            break;
        case GLFW_KEY_R:
            showRearView = !showRearView;
            break;
        case GLFW_KEY_L:
            showLowPressure = !showLowPressure;
            break;
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, true);
            break;
        }
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "F1 Car Visualization", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader carShader("shaders/vertex.glsl", "shaders/fragment.glsl");
    Model carModel("models/f1.obj");
    FlowVisualization flow;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        if (carMoving)
            carPosition += glm::vec3(deltaTime * carSpeed, 0.0f, 0.0f);

        camera.Position = showRearView
            ? carPosition + glm::vec3(-10.0f, 2.0f, 0.0f)
            : carPosition + glm::vec3(0.0f, 2.0f, 10.0f);
        camera.Front = glm::normalize(carPosition - camera.Position);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        carShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, carPosition);

        carShader.setMat4("projection", projection);
        carShader.setMat4("view", view);
        carShader.setMat4("model", model);
        carModel.Draw(carShader);

        flow.render(carPosition, view, projection, showRearView, showLowPressure);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
