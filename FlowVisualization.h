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
    float velocity;                  // Velocity magnitude
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
        m_minDistance = 0.25f;       // Minimum distance between streamlines
        m_adaptiveDensity = true;    // Enable adaptive density

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

                // Calculate new head position with aerodynamic effects
                glm::vec3 displacement = applyAerodynamics(flowLine, distanceToAdvance);
                glm::vec3 newHeadPos = flowLine.points.front() + displacement;

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
        glLineWidth(1.2f);  // Thinner lines for less congestion

        // Enable alpha blending for better visualization
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    // Set adaptive density flag
    void setAdaptiveDensity(bool enable) {
        m_adaptiveDensity = enable;
    }

    // Set minimum distance between streamlines
    void setDensity(float minDistance) {
        m_minDistance = minDistance;
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

        // Determine how many lines to allocate to each zone
        int frontWingLines = m_numLines * 0.3f;  // 30% for front wing
        int topLines = m_numLines * 0.2f;        // 20% for top
        int sideLines = m_numLines * 0.15f;      // 15% for sides
        int rearWingLines = m_numLines * 0.15f;  // 15% for rear wing
        int floorLines = m_numLines * 0.2f;      // 20% for floor/diffuser

        int lineCount = 0;
        std::vector<glm::vec3> existingPositions;

        // Add front wing lines
        for (int i = 0; i < frontWingLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 0;  // Front wing zone

            // Try several positions until we find one with proper spacing
            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-frontWingWidth / 2, frontWingWidth / 2);
                position.y = generateRandomFloat(0.05f, frontWingHeight);
                position.z = frontWingZ - generateRandomFloat(0.0f, 0.2f);

                // Check distance to existing positions
                if (checkMinimumDistance(position, existingPositions, m_minDistance * 0.8f)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.7f, 1.0f);  // Higher pressure in front
                    flowLine.velocity = generateRandomFloat(5.0f, 8.0f);
                    flowLine.speed = flowLine.velocity;
                    flowLine.initialLife = generateRandomFloat(3.0f, 5.0f);
                    flowLine.life = flowLine.initialLife;

                    // Initialize with starting point
                    flowLine.points.push_back(position);
                    flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

                    m_flowLines.push_back(flowLine);
                    existingPositions.push_back(position);
                    positionFound = true;
                    lineCount++;
                }
            }
        }

        // Add top lines
        for (int i = 0; i < topLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 1;  // Top zone

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-topWidth / 2, topWidth / 2);
                position.y = topHeight + generateRandomFloat(0.0f, 0.2f);
                position.z = topZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.3f, 0.6f);  // Medium pressure on top
                    flowLine.velocity = generateRandomFloat(7.0f, 10.0f);
                    flowLine.speed = flowLine.velocity;
                    flowLine.initialLife = generateRandomFloat(3.0f, 5.0f);
                    flowLine.life = flowLine.initialLife;

                    flowLine.points.push_back(position);
                    flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

                    m_flowLines.push_back(flowLine);
                    existingPositions.push_back(position);
                    positionFound = true;
                    lineCount++;
                }
            }
        }

        // Add side lines
        for (int i = 0; i < sideLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 2;  // Side zone

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = (i % 2 == 0) ? sideWidth / 2 : -sideWidth / 2; // Left or right
                position.y = generateRandomFloat(0.2f, sideHeight);
                position.z = sideZ + generateRandomFloat(-m_carLength * 0.2f, m_carLength * 0.2f);

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3((i % 2 == 0) ? 0.2f : -0.2f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.4f, 0.7f);
                    flowLine.velocity = generateRandomFloat(6.0f, 9.0f);
                    flowLine.speed = flowLine.velocity;
                    flowLine.initialLife = generateRandomFloat(3.0f, 5.0f);
                    flowLine.life = flowLine.initialLife;

                    flowLine.points.push_back(position);
                    flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

                    m_flowLines.push_back(flowLine);
                    existingPositions.push_back(position);
                    positionFound = true;
                    lineCount++;
                }
            }
        }

        // Add rear wing lines
        for (int i = 0; i < rearWingLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 3;  // Rear wing zone

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-rearWingWidth / 2, rearWingWidth / 2);
                position.y = generateRandomFloat(rearWingHeight * 0.5f, rearWingHeight);
                position.z = rearWingZ;

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.1f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.1f, 0.4f);  // Lower pressure behind car
                    flowLine.velocity = generateRandomFloat(4.0f, 6.0f);
                    flowLine.speed = flowLine.velocity;
                    flowLine.initialLife = generateRandomFloat(3.0f, 5.0f);
                    flowLine.life = flowLine.initialLife;

                    flowLine.points.push_back(position);
                    flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

                    m_flowLines.push_back(flowLine);
                    existingPositions.push_back(position);
                    positionFound = true;
                    lineCount++;
                }
            }
        }

        // Add floor/diffuser lines
        for (int i = 0; i < floorLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 4;  // Floor zone

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-floorWidth / 2, floorWidth / 2);
                position.y = floorHeight;
                position.z = floorZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);

                if (checkMinimumDistance(position, existingPositions, m_minDistance * 0.7f)) {  // Allow closer spacing under floor
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, -0.05f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.1f, 0.3f);  // Low pressure under floor
                    flowLine.velocity = generateRandomFloat(8.0f, 12.0f);  // Faster flow under floor
                    flowLine.speed = flowLine.velocity;
                    flowLine.initialLife = generateRandomFloat(3.0f, 5.0f);
                    flowLine.life = flowLine.initialLife;

                    flowLine.points.push_back(position);
                    flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));

                    m_flowLines.push_back(flowLine);
                    existingPositions.push_back(position);
                    positionFound = true;
                    lineCount++;
                }
            }
        }
    }

    // Check if a position maintains minimum distance from existing positions
    bool checkMinimumDistance(const glm::vec3& position, const std::vector<glm::vec3>& existingPositions, float minDistance) {
        for (const auto& existingPos : existingPositions) {
            if (glm::length(position - existingPos) < minDistance) {
                return false;
            }
        }
        return true;
    }

    // Apply aerodynamic effects to flow direction
    glm::vec3 applyAerodynamics(const FlowLine& flowLine, float distanceToAdvance) {
        glm::vec3 currentHead = flowLine.points.front();
        glm::vec3 baseDirection = flowLine.direction;

        // Base displacement
        glm::vec3 displacement = baseDirection * distanceToAdvance;

        // 1. Wake effect - streamlines should curve behind the car
        float wakeStrength = 0.0f;
        if (currentHead.z > m_carLength * 0.3f) {  // Behind car
            float distanceFromCenterline = std::abs(currentHead.x);
            wakeStrength = 0.05f * std::exp(-(currentHead.z - m_carLength * 0.3f) / 2.0f);

            // Inward curvature in wake
            if (currentHead.x > 0) {
                displacement.x -= wakeStrength;
            }
            else {
                displacement.x += wakeStrength;
            }

            // Upwash in wake
            displacement.y += wakeStrength * 0.5f;
        }

        // 2. Ground effect - flow accelerates under the car
        if (currentHead.y < m_carHeight * 0.2f &&
            std::abs(currentHead.x) < m_carWidth * 0.4f &&
            std::abs(currentHead.z) < m_carLength * 0.4f) {

            displacement.z *= 1.2f;  // Accelerate flow under car
            displacement.y *= 0.8f;  // Keep flow close to ground
        }

        // 3. Wing vortices - generate tip vortices from wings
        float wingTipDistance = std::min(
            std::abs(currentHead.x - m_carWidth * 0.4f),
            std::abs(currentHead.x + m_carWidth * 0.4f)
        );

        if (wingTipDistance < 0.2f &&
            (std::abs(currentHead.z - m_carLength * 0.4f) < 0.3f ||   // Rear wing
                std::abs(currentHead.z + m_carLength * 0.4f) < 0.3f)) {  // Front wing

            float vortexStrength = 0.08f * (1.0f - wingTipDistance / 0.2f);
            float angle = currentHead.z * 10.0f;  // Rotating angle for vortex

            displacement.x += vortexStrength * std::sin(angle);
            displacement.y += vortexStrength * std::cos(angle);
        }

        // 4. Add small turbulence - but significantly less than before
        float turbulence = 0.01f;  // Reduced turbulence for less chaotic flow
        displacement.x += generateRandomFloat(-turbulence, turbulence);
        displacement.y += generateRandomFloat(-turbulence, turbulence);
        displacement.z += generateRandomFloat(-turbulence, turbulence);

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
        // Calculate velocity magnitude effect (0-1 range)
        float velocityFactor = glm::clamp(flowLine.velocity / 12.0f, 0.0f, 1.0f);

        // Professional CFD color scheme (blue→green→yellow→red)
        glm::vec3 color;

        if (velocityFactor < 0.25f) {
            // Blue to Cyan
            float t = velocityFactor / 0.25f;
            color = glm::vec3(0.0f, t, 1.0f);
        }
        else if (velocityFactor < 0.5f) {
            // Cyan to Green
            float t = (velocityFactor - 0.25f) / 0.25f;
            color = glm::vec3(0.0f, 1.0f, 1.0f - t);
        }
        else if (velocityFactor < 0.75f) {
            // Green to Yellow
            float t = (velocityFactor - 0.5f) / 0.25f;
            color = glm::vec3(t, 1.0f, 0.0f);
        }
        else {
            // Yellow to Red
            float t = (velocityFactor - 0.75f) / 0.25f;
            color = glm::vec3(1.0f, 1.0f - t, 0.0f);
        }

        // Apply fade-out effect based on life ratio
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
    float m_minDistance;       // Minimum distance between streamlines
    bool m_adaptiveDensity;    // Enable adaptive density based on importance
};

#endif // FLOW_VISUALIZATION_H