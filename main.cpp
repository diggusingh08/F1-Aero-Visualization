#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Shader.h"

#include <iostream>
#include <cstdlib>   // For getenv()
#include <direct.h>  // For _getcwd
#include <windows.h> // For Windows-specific path functions

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
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
    GLFWwindow* window = glfwCreateWindow(800, 600, "F1 Car Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 4. Load GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // 5. Print current working directory
    printCurrentDirectory();

    // 6. Load model using Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("C:/Users/hp/Desktop/C assgn/ComputerGraphicsProject/F1_Project_lib/F1_Project_lib/x64/Release/mcl35m_2.obj",
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return -1;
    }

    std::cout << "Model loaded successfully: main.obj" << std::endl;
    std::cout << "Number of meshes: " << scene->mNumMeshes << std::endl;

    // 7. OpenGL settings
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    //Before the render loop
    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl");
    shader.use();


    // 8. Main loop
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Rendering placeholder — future code here

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 9. Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
