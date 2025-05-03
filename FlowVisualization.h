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
    glm::vec3 initialOffset;         // Initial offset from car's reference position
    float lastCarPosition;           // Last known car position for this flow line
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
        m_carPosition = 0.0f;        // Current car Z position
        m_prevCarPosition = 0.0f;    // Previous car Z position
        m_carSpeed = 250.0f;         // Car speed in km/h
        m_simulateDRS = false;       // DRS state
        m_relativeDynamics = true;   // Enable relative dynamics (flow moves with car)

        // Initialize flow line segments
        initFlowLines();
        setupBuffers();
    }

    // Update flow line positions
    void update(float deltaTime) {
        // Calculate car movement delta
        float carMovementDelta = m_carPosition - m_prevCarPosition;
        m_prevCarPosition = m_carPosition;

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

            // If relative dynamics is enabled, adjust for car movement
            if (m_relativeDynamics) {
                // Calculate relative movement delta for this specific flow line
                float relativeDelta = m_carPosition - flowLine.lastCarPosition;
                flowLine.lastCarPosition = m_carPosition;

                // Apply car movement to existing points (they move with the car)
                for (auto& point : flowLine.points) {
                    point.z += relativeDelta;
                }
            }

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

    // Set car position (for moving car functionality)
    void setCarPosition(float position) {
        m_carPosition = position;
    }

    // Set car speed in km/h (affects flow behavior)
    void setCarSpeed(float speed) {
        m_carSpeed = speed;
    }

    // Set DRS state (open/closed)
    void setDRS(bool isOpen) {
        m_simulateDRS = isOpen;
    }

    // Set relative dynamics (flow moves with car)
    void setRelativeDynamics(bool enable) {
        m_relativeDynamics = enable;
    }

    // Reset all flow lines with the current car position
    void resetAllFlowLines() {
        for (auto& flowLine : m_flowLines) {
            resetFlowLine(flowLine);
        }
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
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position

            // Try several positions until we find one with proper spacing
            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-frontWingWidth / 2, frontWingWidth / 2);
                position.y = generateRandomFloat(0.05f, frontWingHeight);
                position.z = frontWingZ - generateRandomFloat(0.0f, 0.2f);

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                // Check distance to existing positions
                if (checkMinimumDistance(position, existingPositions, m_minDistance * 0.8f)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.7f, 1.0f);  // Higher pressure in front
                    flowLine.velocity = generateRandomFloat(5.0f, 8.0f);
                    flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f); // Scale with car speed
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
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-topWidth / 2, topWidth / 2);
                position.y = topHeight + generateRandomFloat(0.0f, 0.2f);
                position.z = topZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.3f, 0.6f);  // Medium pressure on top
                    flowLine.velocity = generateRandomFloat(7.0f, 10.0f);
                    flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f); // Scale with car speed
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
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = (i % 2 == 0) ? sideWidth / 2 : -sideWidth / 2; // Left or right
                position.y = generateRandomFloat(0.2f, sideHeight);
                position.z = sideZ + generateRandomFloat(-m_carLength * 0.2f, m_carLength * 0.2f);

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3((i % 2 == 0) ? 0.2f : -0.2f, 0.0f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.4f, 0.7f);
                    flowLine.velocity = generateRandomFloat(6.0f, 9.0f);
                    flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f); // Scale with car speed
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
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-rearWingWidth / 2, rearWingWidth / 2);
                position.y = generateRandomFloat(rearWingHeight * 0.5f, rearWingHeight);
                position.z = rearWingZ;

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                if (checkMinimumDistance(position, existingPositions, m_minDistance)) {
                    flowLine.initialPosition = position;

                    // Consider DRS state for rear wing flow
                    if (m_simulateDRS) {
                        // DRS open - less drag, straighter flow
                        flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.05f, 1.0f));
                        flowLine.pressure = generateRandomFloat(0.1f, 0.3f);
                        flowLine.velocity = generateRandomFloat(5.0f, 8.0f); // Faster with DRS open
                    }
                    else {
                        // DRS closed - more drag, more turbulent flow
                        flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.1f, 1.0f));
                        flowLine.pressure = generateRandomFloat(0.1f, 0.4f);
                        flowLine.velocity = generateRandomFloat(4.0f, 6.0f);
                    }

                    flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f); // Scale with car speed
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
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position

            bool positionFound = false;
            for (int attempt = 0; attempt < 10 && !positionFound; attempt++) {
                glm::vec3 position;
                position.x = generateRandomFloat(-floorWidth / 2, floorWidth / 2);
                position.y = floorHeight;
                position.z = floorZ + generateRandomFloat(-m_carLength * 0.3f, m_carLength * 0.3f);

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                if (checkMinimumDistance(position, existingPositions, m_minDistance * 0.7f)) {  // Allow closer spacing under floor
                    flowLine.initialPosition = position;
                    flowLine.direction = glm::normalize(glm::vec3(0.0f, -0.05f, 1.0f));
                    flowLine.pressure = generateRandomFloat(0.1f, 0.3f);  // Low pressure under floor
                    flowLine.velocity = generateRandomFloat(8.0f, 12.0f);  // Faster flow under floor
                    flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f); // Scale with car speed
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

        // Calculate relative position to car's current position
        glm::vec3 relativePos = currentHead;
        relativePos.z -= m_carPosition;  // Adjust Z to get position relative to car

        // Apply car speed effects to flow velocity
        float carSpeedFactor = m_carSpeed / 250.0f;
        displacement *= carSpeedFactor;

        // 1. Wake effect - streamlines should curve behind the car
        float wakeStrength = 0.0f;
        if (relativePos.z > m_carLength * 0.3f) {  // Behind car
            float distanceFromCenterline = std::abs(relativePos.x);
            wakeStrength = 0.05f * std::exp(-(relativePos.z - m_carLength * 0.3f) / 2.0f);

            // Inward curvature in wake (stronger at higher car speeds)
            float speedMultiplier = 0.8f + (carSpeedFactor * 0.4f);
            if (relativePos.x > 0) {
                displacement.x -= wakeStrength * speedMultiplier;
            }
            else {
                displacement.x += wakeStrength * speedMultiplier;
            }

            // Upwash in wake - modified by DRS state for rear wing area
            if (m_simulateDRS && std::abs(relativePos.x) < m_carWidth * 0.3f &&
                relativePos.z < m_carLength * 0.6f) {
                displacement.y += wakeStrength * 0.3f * speedMultiplier;  // Less upwash with DRS open
            }
            else {
                displacement.y += wakeStrength * 0.5f * speedMultiplier;  // Normal upwash
            }
        }

        // 2. Ground effect - flow accelerates under the car
        if (relativePos.y < m_carHeight * 0.2f &&
            std::abs(relativePos.x) < m_carWidth * 0.4f &&
            std::abs(relativePos.z) < m_carLength * 0.4f) {

            // Stronger ground effect at higher speeds
            float speedEffect = 1.0f + (carSpeedFactor * 0.5f);
            displacement.z *= 1.2f * speedEffect;  // Accelerate flow under car
            displacement.y *= 0.8f;  // Keep flow close to ground
        }

        // 3. Wing vortices - generate tip vortices from wings
        float wingTipDistance = std::min(
            std::abs(relativePos.x - m_carWidth * 0.4f),
            std::abs(relativePos.x + m_carWidth * 0.4f)
        );

        // Rear wing vortices (stronger at higher speeds)
        if (wingTipDistance < 0.2f && std::abs(relativePos.z - m_carLength * 0.4f) < 0.3f) {
            float vortexStrength = 0.08f * (1.0f - wingTipDistance / 0.2f) * carSpeedFactor;

            // Reduce vortex strength if DRS is open
            if (m_simulateDRS) {
                vortexStrength *= 0.6f;
            }

            float angle = relativePos.z * 10.0f + (m_carSpeed * 0.01f);  // Rotating angle for vortex (affected by speed)
            displacement.x += vortexStrength * std::sin(angle);
            displacement.y += vortexStrength * std::cos(angle);
        }
        // Front wing vortices (stronger at higher speeds)
        else if (wingTipDistance < 0.2f && std::abs(relativePos.z + m_carLength * 0.4f) < 0.3f) {
            float vortexStrength = 0.08f * (1.0f - wingTipDistance / 0.2f) * carSpeedFactor;
            float angle = relativePos.z * 10.0f + (m_carSpeed * 0.01f);  // Rotating angle for vortex
            displacement.x += vortexStrength * std::sin(angle);
            displacement.y += vortexStrength * std::cos(angle);
        }

        // 4. Add small turbulence - increases with car speed
        float turbulence = 0.01f * (0.5f + carSpeedFactor * 0.5f);
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

        // Calculate new position based on car position and initial offset
        glm::vec3 newPos = flowLine.initialOffset + variation;
        newPos.z += m_carPosition;  // Add current car position

        // Update last car position
        flowLine.lastCarPosition = m_carPosition;

        // Add some variation to direction based on car speed
        float speedFactor = m_carSpeed / 250.0f;
        float variationScale = 0.05f + (0.05f * speedFactor);

        glm::vec3 dirVariation(
            generateRandomFloat(-variationScale, variationScale),
            generateRandomFloat(-variationScale, variationScale),
            generateRandomFloat(-variationScale, variationScale)
        );

        flowLine.direction = glm::normalize(flowLine.direction + dirVariation);

        // Update initial position
        flowLine.initialPosition = newPos;

        // Reset life with some variation based on car speed
        flowLine.life = flowLine.initialLife * generateRandomFloat(0.8f, 1.2f);

        // Lower life at higher speeds to simulate faster flow
        flowLine.life *= (1.1f - (speedFactor * 0.2f));

        // Update speed based on car speed with some variation
        float speedVariation = generateRandomFloat(0.9f, 1.1f);
        flowLine.speed = flowLine.velocity * speedFactor * speedVariation;

        // Add starting point
        flowLine.points.push_back(newPos);
        flowLine.colors.push_back(calculateFlowColor(1.0f, 0.0f, flowLine));
    }

    // Calculate color based on flow properties (pressure, velocity, position)
    glm::vec3 calculateFlowColor(float lifeRatio, float pointPosition, const FlowLine& flowLine) {
        // Calculate velocity magnitude effect (0-1 range)
        float velocityFactor = glm::clamp(flowLine.velocity / 12.0f, 0.0f, 1.0f);

        // Adjust color based on car speed (higher speeds = redder colors)
        float speedInfluence = m_carSpeed / 350.0f; // Normalize to 0-1 range (assuming max speed ~350)
        velocityFactor = glm::mix(velocityFactor, velocityFactor + 0.3f, speedInfluence);
        velocityFactor = glm::clamp(velocityFactor, 0.0f, 1.0f);

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

        glBindVertexArray(0);
    }

    // Update OpenGL buffers with new vertex and color data
    void updateBuffers(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& colors) {
        glBindVertexArray(m_VAO);

        // Update vertex positions
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);

        // Update vertex colors
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);

        glBindVertexArray(0);
    }

    // Generate random float between min and max
    float generateRandomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }

private:
    std::vector<FlowLine> m_flowLines;    // Collection of flow lines
    unsigned int m_VAO, m_VBO, m_colorVBO;  // OpenGL objects
    int m_numLines;                       // Number of flow lines
    int m_pointsPerLine;                  // Max points per line
    int m_totalPoints;                    // Total points capacity
    float m_minDistance;                  // Minimum distance between lines
    bool m_adaptiveDensity;               // Enable adaptive density
    float m_carLength;                    // Car dimensions
    float m_carWidth;
    float m_carHeight;
    float m_carPosition;                  // Current car Z position
    float m_prevCarPosition;              // Previous car Z position
    float m_carSpeed;                     // Car speed in km/h
    bool m_simulateDRS;                   // DRS state
    bool m_relativeDynamics;              // Flow moves with car
};

#endif // FLOW_VISUALIZATION_H