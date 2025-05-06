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
    bool isVortex;                   // Flag to indicate if this is a vortex flow line
    float vortexStrength;            // Strength of the vortex rotation (if isVortex is true)
    float vortexPhase;               // Phase of the vortex rotation (if isVortex is true)
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
        m_pointsPerLine = 80;        // Number of points per flow line
        m_totalPoints = m_numLines * m_pointsPerLine;
        m_minDistance = 0.05f;       // Minimum distance between streamlines
        m_adaptiveDensity = true;    // Enable adaptive density
        m_carPosition = -50.0f;        // Current car Z position
        m_prevCarPosition = 0.0f;    // Previous car Z position
        m_carSpeed = 400.0f;         // Car speed in km/h
        m_simulateDRS = false;       // DRS state
        m_relativeDynamics = true;   // Enable relative dynamics (flow moves with car)
        m_visualizePressure = true;  // Show pressure differences in color 
        m_vortexIntensity = 2.0f;    // Vortex visualization intensity

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
                glm::vec3 displacement;
                if (flowLine.isVortex) {
                    displacement = applyVortexMotion(flowLine, distanceToAdvance);
                }
                else {
                    displacement = applyAerodynamics(flowLine, distanceToAdvance);
                }
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
                // Use thicker lines for vortex flows
                if (flowLine.isVortex) {
                    glLineWidth(1.8f);
                }
                else {
                    glLineWidth(1.2f);
                }
                glDrawArrays(GL_LINE_STRIP, offset, flowLine.points.size());
            }
            offset += flowLine.points.size();
        }

        glDisable(GL_LINE_SMOOTH);
        glBindVertexArray(0);
    }

    void drawReferenceMarker(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
        // Initialize buffers if not already done
        static GLuint refVAO = 0, refVBO = 0, refColorVBO = 0;

        if (refVAO == 0) {
            // Create geometry and colors
            std::vector<glm::vec3> referencePoints;
            std::vector<glm::vec3> referenceColors;

            // Vertical pole
            referencePoints.push_back(glm::vec3(0.0f, 0.0f, -m_carLength * 1.0f));
            referencePoints.push_back(glm::vec3(0.0f, 3.0f, -m_carLength * 1.0f));
            referenceColors.push_back(glm::vec3(0.0f, 0.0f, 0.0f)); // Black
            referenceColors.push_back(glm::vec3(0.0f, 0.0f, 0.0f)); // Black

            // Horizontal line
            referencePoints.push_back(glm::vec3(-2.5f, 0.05f, -m_carLength * 1.0f));
            referencePoints.push_back(glm::vec3(2.5f, 0.05f, -m_carLength * 1.0f));
            referenceColors.push_back(glm::vec3(1.0f, 0.0f, 0.0f)); // Red
            referenceColors.push_back(glm::vec3(1.0f, 0.0f, 0.0f)); // Red

            // Create buffers
            glGenVertexArrays(1, &refVAO);
            glGenBuffers(1, &refVBO);
            glGenBuffers(1, &refColorVBO);

            glBindVertexArray(refVAO);

            // Position buffer
            glBindBuffer(GL_ARRAY_BUFFER, refVBO);
            glBufferData(GL_ARRAY_BUFFER, referencePoints.size() * sizeof(glm::vec3), referencePoints.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(0);

            // Color buffer
            glBindBuffer(GL_ARRAY_BUFFER, refColorVBO);
            glBufferData(GL_ARRAY_BUFFER, referenceColors.size() * sizeof(glm::vec3), referenceColors.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(1);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Draw reference marker
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", glm::mat4(1.0f));

        glBindVertexArray(refVAO);
        glLineWidth(3.0f);

        // Draw pole
        glDrawArrays(GL_LINES, 0, 2);

        // Draw horizontal line
        glDrawArrays(GL_LINES, 2, 2);

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
        bool stateChanged = (m_simulateDRS != isOpen);
        m_simulateDRS = isOpen;

        // Regenerate vortices when DRS state changes
        if (stateChanged) {
            regenerateVortices();
        }
    }

    // Set relative dynamics (flow moves with car)
    void setRelativeDynamics(bool enable) {
        m_relativeDynamics = enable;
    }

    // Toggle pressure visualization
    void setPressureVisualization(bool enable) {
        m_visualizePressure = enable;
    }

    // Set vortex visualization intensity (0.0 - 2.0)
    void setVortexIntensity(float intensity) {
        m_vortexIntensity = glm::clamp(intensity, 0.0f, 2.0f);
        regenerateVortices();
    }

    // Reset all flow lines with the current car position
    void resetAllFlowLines() {
        for (auto& flowLine : m_flowLines) {
            resetFlowLine(flowLine);
        }

        // Ensure vortices are properly generated
        regenerateVortices();
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

        // Define reserved lines for vortices
        int vortexLines = m_numLines * 0.1f;  // 10% for vortices

        // Determine how many lines to allocate to each zone
        int frontWingLines = m_numLines * 0.25f;  // 25% for front wing
        int topLines = m_numLines * 0.15f;        // 15% for top
        int sideLines = m_numLines * 0.15f;       // 15% for sides
        int rearWingLines = m_numLines * 0.15f;   // 15% for rear wing
        int floorLines = m_numLines * 0.2f;       // 20% for floor/diffuser

        int lineCount = 0;
        std::vector<glm::vec3> existingPositions;

        // Add front wing lines
        for (int i = 0; i < frontWingLines && lineCount < m_numLines; i++) {
            FlowLine flowLine;
            flowLine.maxPoints = m_pointsPerLine;
            flowLine.zoneType = 0;  // Front wing zone
            flowLine.lastCarPosition = m_carPosition;  // Initialize with current car position
            flowLine.isVortex = false;

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
            flowLine.isVortex = false;

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
            flowLine.isVortex = false;

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
            flowLine.isVortex = false;

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
            flowLine.isVortex = false;

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

        // Add vortex flow lines after normal lines
        regenerateVortices();
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

            // Modify pressure for flow lines under the car
            // This will be used in color calculations
            if (flowLine.zoneType == 4) { // Floor zone
                // Update pressure value for this flow line if it's under the car
                // We'll later use this in color calculations
                const_cast<FlowLine&>(flowLine).pressure =
                    glm::clamp(flowLine.pressure * 0.5f, 0.05f, 0.2f); // Very low pressure under car
            }
        }

        // 3. Wing vortices - generate tip vortices from wings (handled separately in vortex flows)
        float wingTipDistance = std::min(
            std::abs(relativePos.x - m_carWidth * 0.4f),
            std::abs(relativePos.x + m_carWidth * 0.4f)
        );

        // 4. Add small turbulence - increases with car speed
        float turbulence = 0.01f * (0.5f + carSpeedFactor * 0.5f);
        displacement.x += generateRandomFloat(-turbulence, turbulence);
        displacement.y += generateRandomFloat(-turbulence, turbulence);
        displacement.z += generateRandomFloat(-turbulence, turbulence);

        return displacement;
    }

    // Apply vortex motion to vortex flow lines
    glm::vec3 applyVortexMotion(const FlowLine& flowLine, float distanceToAdvance) {
        glm::vec3 currentHead = flowLine.points.front();

        // Calculate relative position to car's current position
        glm::vec3 relativePos = currentHead;
        relativePos.z -= m_carPosition;  // Adjust Z to get position relative to car

        // Base forward motion
        float carSpeedFactor = m_carSpeed / 250.0f;
        glm::vec3 displacement = glm::vec3(0.0f, 0.0f, distanceToAdvance * carSpeedFactor);

        // Calculate vortex rotation
        float vortexPhase = const_cast<FlowLine&>(flowLine).vortexPhase += 0.1f * carSpeedFactor;

        // Apply vortex rotation - depends on the vortex strength and phase
        float rotationRadius = flowLine.vortexStrength * 0.1f * m_vortexIntensity;
        float rotationSpeed = 0.5f + (carSpeedFactor * 0.5f);

        // Vortex motion depends on distance from origin
        float distFromOrigin = glm::length(glm::vec2(relativePos.x, relativePos.y));
        rotationRadius *= (1.0f - std::min(1.0f, distFromOrigin / 2.0f));

        // Apply spiral motion
        displacement.x += rotationRadius * std::cos(vortexPhase * rotationSpeed);
        displacement.y += rotationRadius * std::sin(vortexPhase * rotationSpeed);

        // Add some random turbulence to make it look more realistic
        float turbulence = 0.005f * (0.5f + carSpeedFactor * 0.5f);
        displacement.x += generateRandomFloat(-turbulence, turbulence);
        displacement.y += generateRandomFloat(-turbulence, turbulence);

        return displacement;
    }

    // Generate vortex flow lines around wing tip areas
    void regenerateVortices() {
        // Remove existing vortex lines
        m_flowLines.erase(
            std::remove_if(m_flowLines.begin(), m_flowLines.end(),
                [](const FlowLine& line) { return line.isVortex; }),
            m_flowLines.end()
        );

        // Calculate how many vortex lines to generate
        int vortexLines = std::min(int(m_numLines * 0.1f * m_vortexIntensity), int(m_numLines * 0.2f));

        // Wing dimensions for vortex positioning
        float frontWingZ = -m_carLength * 0.5f;
        float rearWingZ = m_carLength * 0.4f;
        float wingWidth = m_carWidth * 0.9f;
        float rearWingHeight = m_carHeight * 0.9f;
        float frontWingHeight = m_carHeight * 0.3f;

        // Generate vortices at wing tips
        std::vector<std::pair<glm::vec3, float>> vortexPositions;

        // Front wing tip vortices
        vortexPositions.push_back(std::make_pair(
            glm::vec3(wingWidth * 0.5f, frontWingHeight * 0.7f, frontWingZ),
            0.8f  // Strength
        ));
        vortexPositions.push_back(std::make_pair(
            glm::vec3(-wingWidth * 0.5f, frontWingHeight * 0.7f, frontWingZ),
            0.8f  // Strength
        ));

        // Rear wing tip vortices - strength affected by DRS
        float rearVortexStrength = m_simulateDRS ? 0.5f : 1.0f;  // Weaker vortices with DRS open

        vortexPositions.push_back(std::make_pair(
            glm::vec3(wingWidth * 0.45f, rearWingHeight * 0.9f, rearWingZ),
            rearVortexStrength
        ));
        vortexPositions.push_back(std::make_pair(
            glm::vec3(-wingWidth * 0.45f, rearWingHeight * 0.9f, rearWingZ),
            rearVortexStrength
        ));

        // Add DRS-specific vortices when DRS is closed
        if (!m_simulateDRS) {
            // Center vortex from DRS flap trailing edge
            vortexPositions.push_back(std::make_pair(
                glm::vec3(0.0f, rearWingHeight * 0.95f, rearWingZ + 0.1f),
                0.9f
            ));

            // Additional vortices from DRS flap edges
            vortexPositions.push_back(std::make_pair(
                glm::vec3(wingWidth * 0.3f, rearWingHeight * 0.93f, rearWingZ + 0.05f),
                0.7f
            ));
            vortexPositions.push_back(std::make_pair(
                glm::vec3(-wingWidth * 0.3f, rearWingHeight * 0.93f, rearWingZ + 0.05f),
                0.7f
            ));
        }

        // Generate vortex flow lines
        for (int i = 0; i < vortexLines && i < vortexPositions.size(); i++) {
            const glm::vec3& basePosition = vortexPositions[i].first;
            float strength = vortexPositions[i].second;

            // Create multiple flow lines per vortex center
            int linesPerVortex = std::max(1, static_cast<int>(3 * m_vortexIntensity));
            for (int j = 0; j < linesPerVortex && m_flowLines.size() < m_numLines; j++) {
                FlowLine flowLine;
                flowLine.maxPoints = m_pointsPerLine;
                flowLine.zoneType = (basePosition.z < 0) ? 0 : 3;  // Front or rear wing
                flowLine.lastCarPosition = m_carPosition;
                flowLine.isVortex = true;
                flowLine.vortexStrength = strength * (1.0f + generateRandomFloat(-0.2f, 0.2f));
                flowLine.vortexPhase = generateRandomFloat(0.0f, 6.28f);  // Random start phase

                // Add small random offset from vortex center
                glm::vec3 position = basePosition;
                position.x += generateRandomFloat(-0.05f, 0.05f);
                position.y += generateRandomFloat(-0.05f, 0.05f);
                position.z += generateRandomFloat(-0.05f, 0.05f);

                // Store initial offset from car reference position
                flowLine.initialOffset = position;

                // Add car position to get world position
                position.z += m_carPosition;

                flowLine.initialPosition = position;
                flowLine.direction = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));

                // Differentiate vortex pressure values
                if (flowLine.zoneType == 0) {  // Front wing
                    flowLine.pressure = generateRandomFloat(0.2f, 0.4f);  // Lower pressure
                }
                else {  // Rear wing
                    flowLine.pressure = m_simulateDRS ?
                        generateRandomFloat(0.1f, 0.2f) :  // Very low pressure with DRS open
                        generateRandomFloat(0.3f, 0.5f);   // Moderate pressure with DRS closed
                }

                flowLine.velocity = generateRandomFloat(6.0f, 10.0f) * (m_carSpeed / 250.0f);
                flowLine.speed = flowLine.velocity;
                flowLine.initialLife = generateRandomFloat(4.0f, 6.0f);  // Longer life for vortices
                flowLine.life = flowLine.initialLife;

                flowLine.points.push_back(position);

                // Use special color for vortex lines
                glm::vec3 vortexColor = calculateVortexColor(flowLine);
                flowLine.colors.push_back(vortexColor);

                m_flowLines.push_back(flowLine);
            }
        }
    }

    // Calculate flow line color based on pressure, life, and position
    glm::vec3 calculateFlowColor(float lifeRatio, float pointPosition, const FlowLine& flowLine) {
        // Default color scheme based on pressure areas
        glm::vec3 color;

        // Base color selection depends on visualization mode
        if (m_visualizePressure) {
            // Improved pressure-based coloring
            if (flowLine.pressure < 0.2f) {
                // Low pressure - blue tones
                color = glm::vec3(0.0f, 0.3f, 1.0f); // Vibrant blue for very low pressure
            }
            else if (flowLine.pressure < 0.4f) {
                // Medium-low pressure - cyan to teal
                float t = (flowLine.pressure - 0.2f) / 0.2f;
                color = glm::mix(glm::vec3(0.0f, 0.3f, 1.0f), glm::vec3(0.0f, 0.7f, 0.7f), t);
            }
            else if (flowLine.pressure < 0.6f) {
                // Medium pressure - green to yellow
                float t = (flowLine.pressure - 0.4f) / 0.2f;
                color = glm::mix(glm::vec3(0.0f, 0.7f, 0.3f), glm::vec3(0.7f, 0.7f, 0.0f), t);
            }
            else if (flowLine.pressure < 0.8f) {
                // Medium-high pressure - yellow to orange
                float t = (flowLine.pressure - 0.6f) / 0.2f;
                color = glm::mix(glm::vec3(0.7f, 0.7f, 0.0f), glm::vec3(1.0f, 0.5f, 0.0f), t);
            }
            else {
                // High pressure - orange to red
                float t = (flowLine.pressure - 0.8f) / 0.2f;
                color = glm::mix(glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), t);
            }
        }
        else {
            // Zone-based coloring when pressure visualization is off
            switch (flowLine.zoneType) {
            case 0: // Front wing
                color = glm::vec3(0.9f, 0.2f, 0.2f); // Red
                break;
            case 1: // Top
                color = glm::vec3(0.2f, 0.7f, 0.2f); // Green
                break;
            case 2: // Side
                color = glm::vec3(0.2f, 0.5f, 0.9f); // Blue
                break;
            case 3: // Rear wing
                color = glm::vec3(0.9f, 0.7f, 0.2f); // Yellow
                break;
            case 4: // Floor
                color = glm::vec3(0.9f, 0.2f, 0.9f); // Magenta
                break;
            default:
                color = glm::vec3(0.7f); // Gray
            }
        }

        // Apply fade effect based on life and position in line
        float fadeByLife = lifeRatio;
        float fadeByPosition = 1.0f - pointPosition; // Fade tail
        float totalFade = fadeByLife * fadeByPosition;

        // Apply velocity effect to brightness
        float velocityFactor = glm::clamp(flowLine.velocity / 10.0f, 0.5f, 1.5f);
        color *= velocityFactor;

        // Apply additional alpha fade based on total fade value
        float alpha = glm::clamp(totalFade, 0.1f, 1.0f);

        // Return final color with alpha component
        return glm::vec3(color.r, color.g, color.b) * alpha;
    }

    // Special color calculation for vortex flow lines
    glm::vec3 calculateVortexColor(const FlowLine& flowLine) {
        // Vortices get special colors to make them stand out
        glm::vec3 color;

        if (flowLine.zoneType == 0) {  // Front wing vortices
            // Blue-cyan spiral
            color = glm::vec3(0.2f, 0.5f, 1.0f);
        }
        else {  // Rear wing vortices
            if (m_simulateDRS) {
                // DRS open - golden yellow vortices
                color = glm::vec3(1.0f, 0.8f, 0.2f);
            }
            else {
                // DRS closed - reddish orange vortices
                color = glm::vec3(1.0f, 0.4f, 0.1f);
            }
        }

        // Intensity affected by vortex strength
        float intensityFactor = 0.7f + (flowLine.vortexStrength * 0.3f);
        color *= intensityFactor;

        // Make vortices more visible with higher alpha
        float alpha = 0.9f;

        return color * alpha;
    }

    // Reset a flow line to its initial state with updated car position
    void resetFlowLine(FlowLine& flowLine) {
        // Clear existing points
        flowLine.points.clear();
        flowLine.colors.clear();

        // Reset life
        flowLine.life = flowLine.initialLife;

        // Update position based on car's current position
        glm::vec3 newPosition = flowLine.initialOffset;
        newPosition.z += m_carPosition;

        // Update last known car position
        flowLine.lastCarPosition = m_carPosition;

        // Initialize with starting point
        flowLine.points.push_back(newPosition);

        // Calculate initial color based on flow type
        glm::vec3 initialColor;
        if (flowLine.isVortex) {
            initialColor = calculateVortexColor(flowLine);
        }
        else {
            initialColor = calculateFlowColor(1.0f, 0.0f, flowLine);
        }

        flowLine.colors.push_back(initialColor);

        // For vortices, reset phase but keep the strength
        if (flowLine.isVortex) {
            flowLine.vortexPhase = generateRandomFloat(0.0f, 6.28f);
        }

        // Adjust speed based on current car speed
        flowLine.speed = flowLine.velocity * (m_carSpeed / 250.0f);
    }

    // Initialize OpenGL buffers for flow line rendering
    void setupBuffers() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_colorVBO);

        glBindVertexArray(m_VAO);

        // Position buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // Color buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, m_totalPoints * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Update buffer data with current flow line points and colors
    void updateBuffers(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& colors) {
        // Update vertex positions
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);

        // Update vertex colors
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Generate random float in range
    float generateRandomFloat(float min, float max) {
        static std::mt19937 generator(std::random_device{}());
        std::uniform_real_distribution<float> distribution(min, max);
        return distribution(generator);
    }

private:
    // OpenGL buffer objects
    GLuint m_VAO, m_VBO, m_colorVBO;

    // Flow lines data
    std::vector<FlowLine> m_flowLines;
    int m_numLines;
    int m_pointsPerLine;
    int m_totalPoints;

    // Car properties
    float m_carLength;
    float m_carWidth;
    float m_carHeight;
    float m_carPosition;
    float m_prevCarPosition;
    float m_carSpeed;

    // Visualization parameters
    float m_minDistance;
    bool m_adaptiveDensity;
    bool m_simulateDRS;
    bool m_relativeDynamics;
    bool m_visualizePressure;
    float m_vortexIntensity;
};

// FLOW_VISUALIZATIO