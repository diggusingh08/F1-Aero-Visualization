#ifndef FLOW_VISUALIZATION_H
#define FLOW_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Shader.h"

// Flow line segment for visualization
struct FlowLineSegment {
    glm::vec3 position;
    glm::vec3 color;
    float life;
    float initialLife;
    float speed;
    glm::vec3 direction;
};

// Main class for flow line visualization
class FlowLinesVisualization {
public:
    FlowLinesVisualization(int numLines, float carLength, float carWidth, float carHeight) {
        // Initialize flow lines
        m_numLines = numLines;
        m_carLength = carLength;
        m_carWidth = carWidth;
        m_carHeight = carHeight;

        // Initialize flow line segments
        initFlowLines();
        setupBuffers();
    }

    // Update flow line positions
    void update(float deltaTime) {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> colors;

        // For each flow line, update position and add vertices
        for (auto& segment : m_flowSegments) {
            // Update life
            segment.life -= deltaTime;

            // If life is over, reset the segment
            if (segment.life <= 0.0f) {
                resetSegment(segment);
            }

            // Move segment
            segment.position += segment.direction * segment.speed * deltaTime;

            // Add to vertices
            vertices.push_back(segment.position);

            // Calculate color based on life ratio and speed
            float lifeRatio = segment.life / segment.initialLife;
            float speedFactor = segment.speed / 2.0f; // Normalized speed factor

            // Gradient color from red->yellow->green based on life and speed
            glm::vec3 color;
            if (lifeRatio > 0.7f) {
                // Green to yellow
                float t = (lifeRatio - 0.7f) / 0.3f;
                color = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), t);
            }
            else if (lifeRatio > 0.3f) {
                // Yellow to red
                float t = (lifeRatio - 0.3f) / 0.4f;
                color = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), t);
            }
            else {
                // Red with intensity based on remaining life
                color = glm::vec3(1.0f, 0.0f, 0.0f) * (lifeRatio / 0.3f);
            }

            // Adjust color intensity based on speed
            color *= (0.7f + 0.3f * speedFactor);

            // Add color
            colors.push_back(color);
        }

        // Update VBO data
        updateBuffers(vertices, colors);
    }

    // Draw flow lines
    void draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", glm::mat4(1.0f));

        glBindVertexArray(m_VAO);
        glDrawArrays(GL_POINTS, 0, m_flowSegments.size());
        glBindVertexArray(0);
    }

    // Cleanup resources
    void cleanup() {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_colorVBO);
    }

private:
    // Initialize flow lines with random positions around the car
    void initFlowLines() {
        m_flowSegments.clear();

        // Define the emission zone (in front of the car)
        float frontWingZ = -m_carLength * 0.6f;  // Position in front of the car
        float emissionWidth = m_carWidth * 1.5f;
        float emissionHeight = m_carHeight * 1.5f;

        for (int i = 0; i < m_numLines; i++) {
            FlowLineSegment segment;

            // Generate position in front of the car
            float x = generateRandomFloat(-emissionWidth / 2, emissionWidth / 2);
            float y = generateRandomFloat(0.05f, emissionHeight);
            float z = frontWingZ;

            segment.position = glm::vec3(x, y, z);

            // Flow direction (mostly forward with slight variations)
            float dirX = generateRandomFloat(-0.1f, 0.1f);
            float dirY = generateRandomFloat(-0.1f, 0.1f);
            float dirZ = generateRandomFloat(0.9f, 1.0f);  // Mostly forward
            segment.direction = glm::normalize(glm::vec3(dirX, dirY, dirZ));

            // Random speed and life
            segment.speed = generateRandomFloat(1.0f, 2.0f);
            segment.initialLife = generateRandomFloat(2.0f, 4.0f);
            segment.life = segment.initialLife;

            // Initial color (will be set in update)
            segment.color = glm::vec3(0.0f, 1.0f, 0.0f);

            m_flowSegments.push_back(segment);
        }
    }

    // Reset a segment when its life is over
    void resetSegment(FlowLineSegment& segment) {
        // Define the emission zone (in front of the car)
        float frontWingZ = -m_carLength * 0.6f;
        float emissionWidth = m_carWidth * 1.5f;
        float emissionHeight = m_carHeight * 1.5f;

        // Generate new position in front of the car
        float x = generateRandomFloat(-emissionWidth / 2, emissionWidth / 2);
        float y = generateRandomFloat(0.05f, emissionHeight);
        float z = frontWingZ;

        segment.position = glm::vec3(x, y, z);

        // Flow direction (mostly forward with slight variations)
        float dirX = generateRandomFloat(-0.1f, 0.1f);
        float dirY = generateRandomFloat(-0.1f, 0.1f);
        float dirZ = generateRandomFloat(0.9f, 1.0f);  // Mostly forward
        segment.direction = glm::normalize(glm::vec3(dirX, dirY, dirZ));

        // Random speed and life
        segment.speed = generateRandomFloat(1.0f, 2.0f);
        segment.initialLife = generateRandomFloat(2.0f, 4.0f);
        segment.life = segment.initialLife;
    }

    // Setup OpenGL buffers
    void setupBuffers() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_colorVBO);

        glBindVertexArray(m_VAO);

        // Position attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    // Update buffer data
    void updateBuffers(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& colors) {
        glBindVertexArray(m_VAO);

        // Update position data
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);

        // Update color data
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    // Utility: Generate random float in range
    float generateRandomFloat(float min, float max) {
        float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return min + random * (max - min);
    }

private:
    int m_numLines;
    float m_carLength;
    float m_carWidth;
    float m_carHeight;
    std::vector<FlowLineSegment> m_flowSegments;
    GLuint m_VAO, m_VBO, m_colorVBO;
};

#endif // FLOW_VISUALIZATION_H