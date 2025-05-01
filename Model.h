#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "Shader.h"

#include <string>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

class Model {
public:
    // Constructor
    Model(const std::string& path);

    // Draw the model
    void Draw(Shader& shader);

private:
    // Model data
    std::vector<Mesh> meshes;
    std::string directory;

    // Loads a model with supported ASSIMP extensions
    void loadModel(const std::string& path);

    // Recursive processing
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
};

#endif
