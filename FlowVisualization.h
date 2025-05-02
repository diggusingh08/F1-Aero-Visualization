#ifndef FLOW_VISUALIZATION_H
#define FLOW_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>
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
    float pressure;                  // Pressure value for coloring
    float temperature;               // Temperature value for coloring
    int zoneType;                    // The emission zone this flow line came from
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
                glm::vec3 newHeadPos = flowLine.points.front() + applyAerodynamics(flowLine, distanceToAdvance);

                // Insert new head at the beginning
                flowLine.points.insert(flowLine.points.begin(), newHeadPos);

                // Calculate color for the new head
                float lifeRatio = flowLine.life / flowLine.initialLife;
                float pointPosition = 0.0f; // Head position is 0
                glm::vec3 color = calculateFlowColor(lifeRatio, pointPosition, flowLine);
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

        // Floor/diffuser
        float floorZ = 0.0f;
        float floorWidth = m_carWidth * 0.8f;
        float floorHeight = 0.05f;

        for (int i = 0; i < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;

            // Choose a random emission zone
            int zone = i % 5;  // Now including the floor zone
            flowLine.zoneType = zone;

            glm::vec3 position;
            glm::vec3 direction;

            switch (zone) {
            case 0: // Front wing
                position.x = generateRandomFloat(-frontWingWidth / 2, frontWingWidth / 2);
                position.y = generateRandomFloat(0.05f, frontWingHeight);
                position.z = frontWingZ - generateRandomFloat(0.0f, 0.2f);
                direction = glm::vec3(0.0f, 0.0f, 1.0f); // Flow from front to back
                flowLine.pressure = generateRandomFloat(0.7f, 1.0f);  // Higher pressure in front
                flowLine.temperature = generateRandomFloat(0.4f, 0.6f);
                flowLine.speed = generateRandomFloat(5.0f, 8.0f);
                break;

            case 1: // Top of car
                position.x = generateRandomFloat(-topWidth / 2, topWidth / 2);
                position.y = topHeight + generateRandomFloat(0.0f, 0.2f);
                position.z = topZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);
                direction = glm::vec3(0.0f, 0.0f, 1.0f); // Flow from front to back
                flowLine.pressure = generateRandomFloat(0.3f, 0.6f);  // Medium pressure on top
                flowLine.temperature = generateRandomFloat(0.5f, 0.7f);
                flowLine.speed = generateRandomFloat(7.0f, 10.0f);
                break;

            case 2: // Side pods
                position.x = (i % 2 == 0) ? sideWidth / 2 : -sideWidth / 2; // Left or right
                position.y = generateRandomFloat(0.2f, sideHeight);
                position.z = sideZ + generateRandomFloat(-m_carLength * 0.2f, m_carLength * 0.2f);
                direction = glm::vec3((i % 2 == 0) ? 0.2f : -0.2f, 0.0f, 1.0f); // Flow outward and back
                flowLine.pressure = generateRandomFloat(0.4f, 0.7f);
                flowLine.temperature = generateRandomFloat(0.6f, 0.8f); // Higher temp near sidepods (radiators)
                flowLine.speed = generateRandomFloat(6.0f, 9.0f);
                break;

            case 3: // Rear wing
                position.x = generateRandomFloat(-rearWingWidth / 2, rearWingWidth / 2);
                position.y = generateRandomFloat(rearWingHeight * 0.5f, rearWingHeight);
                position.z = rearWingZ;
                direction = glm::vec3(0.0f, 0.1f, 1.0f); // Flow upward and back
                flowLine.pressure = generateRandomFloat(0.1f, 0.4f);  // Lower pressure behind car
                flowLine.temperature = generateRandomFloat(0.3f, 0.5f);
                flowLine.speed = generateRandomFloat(4.0f, 6.0f);
                break;

            case 4: // Floor/diffuser
                position.x = generateRandomFloat(-floorWidth / 2, floorWidth / 2);
                position.y = floorHeight;
                position.z = floorZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);
                direction = glm::vec3(0.0f, -0.1f, 1.0f); // Flow downward and back (ground effect)
                flowLine.pressure = generateRandomFloat(0.1f, 0.3f);  // Low pressure under floor
                flowLine.temperature = generateRandomFloat(0.2f, 0.4f);
                flowLine.speed = generateRandomFloat(8.0f, 12.0f);  // Faster flow under floor
                break;
            }

            flowLine.initialPosition = position;
            flowLine.direction = glm::normalize(direction);

            // Initial life value
            flowLine.initialLife = generateRandomFloat(3.0f, 6.0f);
            flowLine.life = flowLine.initialLife;

            // Initialize with starting point
            flowLine.points.push_back(position);
            flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

            m_flowLines.push_back(flowLine);
        }
    }

    // Apply aerodynamic effects to flow direction
    glm::vec3 applyAerodynamics(FlowLine& flowLine, float distanceToAdvance) {
        glm::vec3 currentHead = flowLine.points.front();
        glm::vec3 displacement = flowLine.direction * distanceToAdvance;

        // Base turbulence
        float turbulenceStrength = 0.03f;
        displacement.x += generateRandomFloat(-turbulenceStrength, turbulenceStrength);
        displacement.y += generateRandomFloat(-turbulenceStrength, turbulenceStrength);
        displacement.z += generateRandomFloat(-turbulenceStrength, turbulenceStrength);

        // Apply vortex effects at wing tips
        if (flowLine.zoneType == 0) { // Front wing
            float distFromWingTip = std::min(
                std::abs(currentHead.x - m_carWidth * 0.6f),
                std::abs(currentHead.x + m_carWidth * 0.6f)
            );

            if (distFromWingTip < 0.3f) {
                // Create wing tip vortex
                float vortexStrength = 0.2f * (1.0f - distFromWingTip / 0.3f);
                displacement.y += vortexStrength * std::sin(flowLine.life * 5.0f);
                displacement.x += vortexStrength * std::cos(flowLine.life * 5.0f) *
                    (currentHead.x > 0 ? -1.0f : 1.0f); // Direction based on which wing tip
            }
        }

        // Wake turbulence behind the car
        if (currentHead.z > m_carLength * 0.5f) {
            float wakeStrength = 0.1f * std::exp(-(currentHead.z - m_carLength * 0.5f));
            displacement.x += generateRandomFloat(-wakeStrength, wakeStrength);
            displacement.y += generateRandomFloat(-wakeStrength, wakeStrength);
        }

        // DRS effect when open
        if (flowLine.zoneType == 3) { // Rear wing
            // Simulate DRS being open - less upwash
            displacement.y *= 0.7f;
            displacement.z *= 1.3f; // Faster flow with less drag
        }

        // Ground effect for floor/diffuser
        if (flowLine.zoneType == 4) {
            // Stronger downforce as flow accelerates through venturi tunnels
            if (currentHead.z > -m_carLength * 0.3f && currentHead.z < m_carLength * 0.3f) {
                displacement.y -= 0.05f;
                displacement.z *= 1.2f; // Accelerated flow
            }
        }

        return displacement;
    }

    // Reset a flow line to its initial state with some variation
    void resetFlowLine(FlowLine& flowLine) {
        flowLine.points.clear();
        flowLine.colors.clear();

        // Add some variation to starting position
        glm::vec3 variation(
            generateRandomFloat(-0.1f, 0.1f),
            generateRandomFloat(-0.1f, 0.1f),
            generateRandomFloat(-0.1f, 0.1f)
        );

        glm::vec3 newPos = flowLine.initialPosition + variation;

        // Add some variation to direction
        glm::vec3 dirVariation(
            generateRandomFloat(-0.05f, 0.05f),
            generateRandomFloat(-0.05f, 0.05f),
            generateRandomFloat(-0.05f, 0.05f)
        );

        flowLine.direction = glm::normalize(flowLine.direction + dirVariation);

        // Reset life
        flowLine.life = flowLine.initialLife * generateRandomFloat(0.8f, 1.2f);

        // Add starting point
        flowLine.points.push_back(newPos);
        flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));
    }

    // Calculate color based on flow properties (pressure, velocity, position)
    glm::vec3 calculateFlowColor(float lifeRatio, float pointPosition, const FlowLine& flowLine) {
        // Base the color on a combination of factors:
        // 1. Pressure (red = high pressure, blue = low pressure)
        // 2. Position along flow line (alpha fade out)
        // 3. Flow speed

        // Use pressure to determine the base color
        glm::vec3 color;

        // Use a heat map style coloring similar to professional CFD
        float value = flowLine.pressure; // Range 0.0 - 1.0

        // Red to Yellow to Green to Blue gradient
        if (value < 0.25f) {
            // Blue to Green (0.0 - 0.25)
            float t = value / 0.25f;
            color = glm::vec3(0.0f, t, 1.0f - t * 0.5f);
        }
        else if (value < 0.5f) {
            // Green to Yellow (0.25 - 0.5)
            float t = (value - 0.25f) / 0.25f;
            color = glm::vec3(t, 1.0f, 0.5f - t * 0.5f);
        }
        else if (value < 0.75f) {
            // Yellow to Orange (0.5 - 0.75)
            float t = (value - 0.5f) / 0.25f;
            color = glm::vec3(1.0f, 1.0f - t * 0.5f, 0.0f);
        }
        else {
            // Orange to Red (0.75 - 1.0)
            float t = (value - 0.75f) / 0.25f;
            color = glm::vec3(1.0f, 0.5f - t * 0.5f, 0.0f);
        }

        // Adjust color based on temperature (more intense colors for higher temps)
        color = glm::mix(color, glm::vec3(1.0f, 0.5f, 0.0f), flowLine.temperature * 0.3f);

        // Fade out based on life ratio
        color *= lifeRatio;

        return color;
    }

    // Set up OpenGL buffers
    void setupBuffers() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_colorVBO);

        glBindVertexArray(m_VAO);

        // Position attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Update buffer data
    void updateBuffers(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& colors) {
        glBindVertexArray(m_VAO);

        // Update position data
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), vertices.data());

        // Update color data
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, colors.size() * sizeof(glm::vec3), colors.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Generate random float in range
    float generateRandomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }

private:
    std::vector<FlowLine> m_flowLines;
    unsigned int m_VAO, m_VBO, m_colorVBO;
    int m_numLines;
    int m_pointsPerLine;
    int m_totalPoints;
    float m_carLength;
    float m_carWidth;
    float m_carHeight;
};

#endif // FLOW_VISUALIZATION_H