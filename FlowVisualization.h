#ifndef FLOW_VISUALIZATION_H
#define FLOW_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Shader.h"

// Flow line for visualization
struct FlowLine {
    std::vector<glm::vec3> points;   // Points forming the flow line
    std::vector<glm::vec3> colors;   // Color for each point
    float life;                      // Life of the entire flow line
    float initialLife;               // Initial life value
    float speed;                     // Speed of flow line progression
    glm::vec3 initialPosition;       // Initial starting position
    glm::vec3 direction;             // Main flow direction
    int maxPoints;                   // Maximum number of points in line
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
        m_pointsPerLine = 50;        // Number of points per flow line
        m_totalPoints = m_numLines * m_pointsPerLine;

        // Initialize flow line segments
        initFlowLines();
        setupBuffers();
    }

    // Update flow line positions
    void update(float deltaTime) {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> colors;

        // For each flow line
        for (auto& flowLine : m_flowLines) {
            // Update life
            flowLine.life -= deltaTime;

            // If life is over, reset the flow line
            if (flowLine.life <= 0.0f) {
                resetFlowLine(flowLine);
            }

            // Calculate how much to advance the flow line
            float distanceToAdvance = flowLine.speed * deltaTime;

            // Shift all points forward
            if (flowLine.points.size() > 0) {
                // Remove tail if we reached max points
                if (flowLine.points.size() >= flowLine.maxPoints) {
                    flowLine.points.pop_back();
                    flowLine.colors.pop_back();
                }

                // Calculate new head position by advancing from current head
                glm::vec3 newHeadPos = flowLine.points.front() + flowLine.direction * distanceToAdvance;

                // Add turbulence/variation to the flow
                float turbulenceStrength = 0.03f;
                newHeadPos.x += generateRandomFloat(-turbulenceStrength, turbulenceStrength);
                newHeadPos.y += generateRandomFloat(-turbulenceStrength, turbulenceStrength);
                newHeadPos.z += generateRandomFloat(-turbulenceStrength, turbulenceStrength);

                // Insert new head at the beginning
                flowLine.points.insert(flowLine.points.begin(), newHeadPos);

                // Calculate color for the new head
                float lifeRatio = flowLine.life / flowLine.initialLife;
                float pointPosition = 0.0f; // Head position is 0
                glm::vec3 color = calculateFlowColor(lifeRatio, pointPosition, flowLine.speed);
                flowLine.colors.insert(flowLine.colors.begin(), color);
            }

            // Add all points and colors to rendering vectors
            for (size_t i = 0; i < flowLine.points.size(); i++) {
                vertices.push_back(flowLine.points[i]);
                colors.push_back(flowLine.colors[i]);
            }
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

        // Enable line smoothing and set line width
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.5f);

        // Draw lines instead of points
        int offset = 0;
        for (const auto& flowLine : m_flowLines) {
            if (flowLine.points.size() > 1) {
                glDrawArrays(GL_LINE_STRIP, offset, flowLine.points.size());
            }
            offset += flowLine.points.size();
        }

        glDisable(GL_LINE_SMOOTH);
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
        m_flowLines.clear();

        // Define the emission zones around the car
        // Front wing
        float frontWingZ = -m_carLength * 0.5f;
        float frontWingWidth = m_carWidth * 1.2f;
        float frontWingHeight = m_carHeight * 0.3f;

        // Top of car (airbox/engine cover)
        float topZ = 0.0f;
        float topWidth = m_carWidth * 0.5f;
        float topHeight = m_carHeight;

        // Side pods
        float sideZ = 0.0f;
        float sideWidth = m_carWidth * 0.5f;
        float sideHeight = m_carHeight * 0.5f;

        // Rear wing
        float rearWingZ = m_carLength * 0.4f;
        float rearWingWidth = m_carWidth * 0.9f;
        float rearWingHeight = m_carHeight * 0.9f;

        for (int i = 0; i < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;

            // Choose a random emission zone
            int zone = i % 4;

            glm::vec3 position;
            glm::vec3 direction;

            switch (zone) {
            case 0: // Front wing
                position.x = generateRandomFloat(-frontWingWidth / 2, frontWingWidth / 2);
                position.y = generateRandomFloat(0.05f, frontWingHeight);
                position.z = frontWingZ;

                // Flow direction (mostly backward with upwash or downwash)
                direction.x = generateRandomFloat(-0.2f, 0.2f);
                direction.y = generateRandomFloat(-0.3f, 0.5f); // Mix of downforce and upwash
                direction.z = generateRandomFloat(0.8f, 1.0f);  // Mostly backward
                break;

            case 1: // Top of car
                position.x = generateRandomFloat(-topWidth / 2, topWidth / 2);
                position.y = generateRandomFloat(topHeight * 0.7f, topHeight);
                position.z = topZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);

                // Flow direction (mostly backward with slight upwash)
                direction.x = generateRandomFloat(-0.2f, 0.2f);
                direction.y = generateRandomFloat(0.0f, 0.3f); // Slight upwash
                direction.z = generateRandomFloat(0.8f, 1.0f); // Mostly backward
                break;

            case 2: // Side pods
                position.x = (i % 2 == 0) ? sideWidth : -sideWidth;
                position.y = generateRandomFloat(0.1f, sideHeight);
                position.z = sideZ + generateRandomFloat(-m_carLength * 0.2f, m_carLength * 0.2f);

                // Flow direction (outward and backward)
                direction.x = (position.x > 0) ? generateRandomFloat(0.3f, 0.5f) : generateRandomFloat(-0.5f, -0.3f);
                direction.y = generateRandomFloat(-0.1f, 0.2f);
                direction.z = generateRandomFloat(0.7f, 0.9f); // Mostly backward
                break;

            case 3: // Rear wing
                position.x = generateRandomFloat(-rearWingWidth / 2, rearWingWidth / 2);
                position.y = generateRandomFloat(rearWingHeight * 0.5f, rearWingHeight);
                position.z = rearWingZ;

                // Flow direction (mostly backward with upwash)
                direction.x = generateRandomFloat(-0.2f, 0.2f);
                direction.y = generateRandomFloat(0.2f, 0.5f); // Upwash from rear wing
                direction.z = generateRandomFloat(0.7f, 0.9f); // Mostly backward
                break;
            }

            flowLine.initialPosition = position;
            flowLine.direction = glm::normalize(direction);

            // Random speed and life
            flowLine.speed = generateRandomFloat(3.0f, 5.0f);
            flowLine.initialLife = generateRandomFloat(3.0f, 6.0f);
            flowLine.life = flowLine.initialLife;

            // Initialize with a single point
            flowLine.points.push_back(position);
            flowLine.colors.push_back(glm::vec3(0.0f, 1.0f, 0.0f)); // Initial color

            m_flowLines.push_back(flowLine);
        }
    }

    // Reset a flow line when its life is over
    void resetFlowLine(FlowLine& flowLine) {
        // Clear existing points
        flowLine.points.clear();
        flowLine.colors.clear();

        // Reset position to initial emission point
        glm::vec3 position = flowLine.initialPosition;

        // Slightly vary the direction and position for diversity
        float dirX = generateRandomFloat(-0.2f, 0.2f);
        float dirY = generateRandomFloat(-0.2f, 0.2f);
        float dirZ = generateRandomFloat(0.8f, 1.0f);
        flowLine.direction = glm::normalize(glm::vec3(dirX, dirY, dirZ));

        // Random speed and life
        flowLine.speed = generateRandomFloat(3.0f, 5.0f);
        flowLine.initialLife = generateRandomFloat(3.0f, 6.0f);
        flowLine.life = flowLine.initialLife;

        // Add initial point
        flowLine.points.push_back(position);
        flowLine.colors.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Calculate color based on parameters
    glm::vec3 calculateFlowColor(float lifeRatio, float pointPosition, float speed) {
        // Normalize point position in [0, 1] range (0 is head, 1 is tail)
        float normalizedPosition = glm::clamp(pointPosition, 0.0f, 1.0f);

        // Fade alpha at the tail end
        float speedFactor = speed / 5.0f; // Normalize speed

        // Create a vibrant color scheme like in the reference image
        glm::vec3 color;

        // Color gradient based on position in the line (head to tail)
        if (normalizedPosition < 0.3f) {
            // Head: Green to yellow transition
            float t = normalizedPosition / 0.3f;
            color = glm::mix(glm::vec3(0.0f, 1.0f, 0.2f), glm::vec3(1.0f, 1.0f, 0.0f), t);
        }
        else if (normalizedPosition < 0.6f) {
            // Middle: Yellow to orange transition
            float t = (normalizedPosition - 0.3f) / 0.3f;
            color = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.5f, 0.0f), t);
        }
        else {
            // Tail: Orange to cyan (for blue trails in image)
            float t = (normalizedPosition - 0.6f) / 0.4f;
            color = glm::mix(glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(0.0f, 0.7f, 1.0f), t);
        }

        // Add slight brightness variation based on speed
        color *= (0.7f + 0.3f * speedFactor);

        // Fade overall brightness with life ratio
        color *= (0.5f + 0.5f * lifeRatio);

        return color;
    }

    // Setup OpenGL buffers
    void setupBuffers() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_colorVBO);

        glBindVertexArray(m_VAO);

        // Position attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
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
    int m_pointsPerLine;
    int m_totalPoints;
    float m_carLength;
    float m_carWidth;
    float m_carHeight;
    std::vector<FlowLine> m_flowLines;
    GLuint m_VAO, m_VBO, m_colorVBO;
};

#endif // FLOW_VISUALIZATION_H