#ifndef FLOW_LINES_VISUALIZATION_H
#define FLOW_LINES_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <random>
#include "Shader.h"

// Structure to hold flow line data
struct FlowLine {
    std::vector<glm::vec3> points;    // Series of points forming the line
    std::vector<glm::vec3> colors;    // Color for each point (allows gradient effects)
    float life;                       // Life of the flow line
    float maxLife;                    // Maximum life span
    float pressure;                   // Store pressure value for visualization
    bool active;                      // Is the line currently active
};

class FlowLinesVisualization {
public:
    // Constructor
    FlowLinesVisualization(int numLines, float carLength, float carWidth, float carHeight) {
        init(numLines, carLength, carWidth, carHeight);
    }

    // 4. Add a new method to initialize a more even distribution of flow lines 
    // Initialize flow lines in a grid pattern on the front wing
    void initFlowLinesGrid() {
        // Clear any existing flow lines
        flowLines.clear();
        flowLines.resize(numLines);

        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());

        // Organized grid of starting points across the front wing
        int gridWidth = static_cast<int>(std::sqrt(numLines));
        int gridHeight = numLines / gridWidth;

        // Front wing parameters
        float frontWingZ = -carLength * 0.45f;
        float frontWingY = carHeight * 0.2f;
        float frontWingWidth = carWidth * 0.9f;

        // Create flow lines in a grid pattern
        int lineIndex = 0;
        for (int i = 0; i < gridWidth && lineIndex < numLines; i++) {
            for (int j = 0; j < gridHeight && lineIndex < numLines; j++) {
                // Calculate normalized position (0.0 to 1.0)
                float normalizedX = static_cast<float>(i) / (gridWidth - 1);
                float normalizedY = static_cast<float>(j) / (gridHeight - 1);

                // Map to actual position
                float x = (normalizedX * 2.0f - 1.0f) * (frontWingWidth * 0.45f);
                float y = normalizedY * frontWingY + 0.05f;

                // Add small random variation
                std::uniform_real_distribution<float> varX(-0.05f, 0.05f);
                std::uniform_real_distribution<float> varY(-0.03f, 0.03f);
                std::uniform_real_distribution<float> varZ(-0.05f, 0.05f);

                // Create starting point at front wing
                glm::vec3 startPoint = glm::vec3(
                    x + varX(gen),
                    y + varY(gen),
                    frontWingZ + varZ(gen)
                );

                // Initialize the flow line
                flowLines[lineIndex].points.clear();
                flowLines[lineIndex].colors.clear();
                flowLines[lineIndex].points.push_back(startPoint);
                flowLines[lineIndex].colors.push_back(glm::vec3(0.1f, 0.4f, 0.9f));

                // Set life values
                std::uniform_real_distribution<float> lifeDist(2.0f, 5.0f);
                flowLines[lineIndex].maxLife = lifeDist(gen);
                flowLines[lineIndex].life = flowLines[lineIndex].maxLife;

                // Initial pressure and activation
                flowLines[lineIndex].pressure = 1.0f;
                flowLines[lineIndex].active = true;

                lineIndex++;
            }
        }

        // Fill any remaining lines with random positions
        std::uniform_real_distribution<float> xDist(-frontWingWidth * 0.45f, frontWingWidth * 0.45f);
        std::uniform_real_distribution<float> yDist(0.05f, frontWingY + 0.1f);
        std::uniform_real_distribution<float> zDist(-0.05f, 0.05f);

        for (; lineIndex < numLines; lineIndex++) {
            glm::vec3 startPoint = glm::vec3(
                xDist(gen),
                yDist(gen),
                frontWingZ + zDist(gen)
            );

            // Initialize line
            flowLines[lineIndex].points.clear();
            flowLines[lineIndex].colors.clear();
            flowLines[lineIndex].points.push_back(startPoint);
            flowLines[lineIndex].colors.push_back(glm::vec3(0.1f, 0.4f, 0.9f));

            // Set life values
            std::uniform_real_distribution<float> lifeDist(2.0f, 5.0f);
            flowLines[lineIndex].maxLife = lifeDist(gen);
            flowLines[lineIndex].life = flowLines[lineIndex].maxLife;

            flowLines[lineIndex].pressure = 1.0f;
            flowLines[lineIndex].active = true;
        }
    }


    void init(int numLines, float carLength, float carWidth, float carHeight) {
        // Store car dimensions for flow calculations
        this->carLength = carLength;
        this->carWidth = carWidth;
        this->carHeight = carHeight;
        this->numLines = numLines;
        this->maxPoints = 50;  // Maximum points per line

        // Define DRS position for pressure change detection
        drsPositionZ = carLength * 0.4f;

        // Initialize the flow lines using the grid method
        flowLines.resize(numLines);
        initFlowLinesGrid();

        // Create and setup buffers
        setupBuffers();
    }

    // Reset a flow line - start from front wing area
    void resetFlowLine(FlowLine& line, std::mt19937& gen) {
        // Clear previous points and colors
        line.points.clear();
        line.colors.clear();

        // Start with a single point at the front wing area
        glm::vec3 startPoint = generateFrontWingStartPoint(gen);
        line.points.push_back(startPoint);

        // Initial color - blue to indicate cool, low pressure areas
        line.colors.push_back(glm::vec3(0.1f, 0.4f, 0.9f));

        // Set initial life values
        std::uniform_real_distribution<float> lifeDist(2.0f, 5.0f);
        line.maxLife = lifeDist(gen);
        line.life = line.maxLife;

        // Initial pressure (normal atmosphere)
        line.pressure = 1.0f;

        // Mark as active
        line.active = true;
    }

    // 1. Update the generateFrontWingStartPoint function to ensure proper positioning
    glm::vec3 generateFrontWingStartPoint(std::mt19937& gen) {
        // Front wing position
        float frontWingZ = -carLength * 0.45f;  // Z is length axis - front of the car
        float frontWingY = carHeight * 0.2f;    // Height of front wing

        // Distribution for positions along the front wing
        std::uniform_real_distribution<float> xDist(-carWidth * 0.45f, carWidth * 0.45f);
        std::uniform_real_distribution<float> yDist(0.05f, frontWingY + 0.1f);  // Adjusted height range
        std::uniform_real_distribution<float> zVariation(-0.05f, 0.05f);  // Smaller z variation

        // Generate start point at front wing
        return glm::vec3(
            xDist(gen),                    // Position across width of wing
            yDist(gen),                    // Slight variation in height
            frontWingZ + zVariation(gen)   // Position along length with slight variation
        );
    }

    // Random float helper between -1 and 1
    float randomFloat(std::mt19937& gen) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return dist(gen);
    }

    // Setup OpenGL buffers for rendering
    void setupBuffers() {
        // Generate vertex arrays and buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        // Bind vertex array
        glBindVertexArray(VAO);

        // Allocate buffer space (we'll update this each frame)
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // Pre-allocate a large buffer - 6 floats per vertex (position + color) * max vertices per line * number of lines
        glBufferData(GL_ARRAY_BUFFER, 6 * maxPoints * numLines * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Unbind
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // 2. Modify the update function to ensure continuous flow
    void update(float deltaTime) {
        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());

        for (auto& line : flowLines) {
            // Decrease life
            line.life -= deltaTime;

            if (line.life <= 0.0f) {
                // Reset line if it's dead
                resetFlowLine(line, gen);
            }
            else if (line.active) {
                // Only process active lines

                // Get the last point of the line
                if (!line.points.empty()) {
                    glm::vec3 lastPoint = line.points.back();
                    glm::vec3 velocity = calculateVelocityAtPoint(lastPoint);

                    // Add a new point if the line isn't too long
                    if (line.points.size() < maxPoints) {
                        // Calculate new position using velocity - adjusted speed for smoother lines
                        glm::vec3 newPoint = lastPoint + velocity * deltaTime * 3.0f;  // Increased speed multiplier

                        // Check if the new point is too far away or off-screen
                        if (isPointValid(newPoint)) {
                            line.points.push_back(newPoint);

                            // Calculate pressure for color
                            float pressure = calculatePressureAtPoint(newPoint);
                            line.pressure = pressure;

                            // Set color based on pressure and position
                            glm::vec3 color = calculateColorForPoint(newPoint, pressure, line.points.size() > 1);
                            line.colors.push_back(color);
                        }
                        else {
                            // If the point is invalid, deactivate the line but keep it visible
                            line.active = false;
                        }
                    }
                }
            }
        }
    }

    // Calculate velocity vector at a given point
    glm::vec3 calculateVelocityAtPoint(const glm::vec3& point) {
        // Base velocity - moving backward along Z-axis
        glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 1.0f);

        // Apply car body effect
        applyCarBodyEffect(point, velocity);

        // Apply front wing effect
        applyFrontWingEffect(point, velocity);

        // Apply rear wing effect
        applyRearWingEffect(point, velocity);

        // Apply floor effect
        applyFloorEffect(point, velocity);

        // Normalize and scale for consistent line growth speed
        return glm::normalize(velocity) * 1.5f;
    }

    // Apply car body aerodynamic effect to velocity
    void applyCarBodyEffect(const glm::vec3& point, glm::vec3& velocity) {
        // Car body as a box
        float carZ = 0.0f; // Car centered at origin

        // Check if point is near the car body
        if (point.z > carZ - carLength * 0.5f &&
            point.z < carZ + carLength * 0.5f &&
            point.y < carHeight &&
            std::abs(point.x) < carWidth * 0.5f) {

            // Direction away from car body
            glm::vec3 awayFromCar;

            // Top surface - upward flow
            if (point.y > carHeight * 0.8f) {
                awayFromCar = glm::normalize(glm::vec3(0.5f, 1.0f, point.x * 0.2f));
            }
            // Bottom surface - downward flow with ground effect
            else if (point.y < carHeight * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(1.0f, -0.2f, point.x * 0.1f));
            }
            // Side surfaces - outward flow
            else if (std::abs(point.x) > carWidth * 0.4f) {
                float sideDir = point.x > 0 ? 1.0f : -1.0f;
                awayFromCar = glm::normalize(glm::vec3(sideDir, 0.1f, 0.5f));
            }
            // Front of car - flows around
            else if (point.z < -carLength * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(
                    point.x * 0.7f,
                    point.y < carHeight * 0.5f ? -0.3f : 0.3f,
                    0.2f
                ));
            }
            // Rear of car - flows behind
            else if (point.z > carLength * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(
                    point.x * 0.3f,
                    point.y < carHeight * 0.5f ? -0.2f : 0.2f,
                    1.0f
                ));
            }
            // Middle of car
            else {
                awayFromCar = glm::normalize(glm::vec3(
                    point.x,
                    point.y < carHeight * 0.5f ? -0.3f : 0.3f,
                    0.7f
                ));
            }

            // Apply force away from car
            velocity += awayFromCar * 1.5f;
        }
    }

    // 3. Update applyFrontWingEffect to better guide flow lines from front to back
    void applyFrontWingEffect(const glm::vec3& point, glm::vec3& velocity) {
        // Front wing position
        float frontWingZ = -carLength * 0.4f;
        float frontWingY = carHeight * 0.2f;
        float frontWingWidth = carWidth * 0.9f;

        // Check if point is near the front wing with expanded detection area
        if (point.z > frontWingZ - carLength * 0.15f &&
            point.z < frontWingZ + carLength * 0.15f &&
            point.y < frontWingY + carHeight * 0.15f &&
            std::abs(point.x) < frontWingWidth * 0.5f) {

            // Enhanced backward flow to ensure lines travel along car length
            velocity.z += 0.7f;  // Increased backward flow component

            // Downforce varies based on position
            if (point.y < frontWingY * 0.7f) {
                // Stronger downforce under the wing
                velocity.y -= 0.5f;
            }
            else {
                // Less downforce or slight upforce over the wing
                velocity.y += 0.2f;
            }

            // Create wing-tip vortices
            if (std::abs(point.x) > frontWingWidth * 0.4f) {
                float vortexDir = point.x > 0 ? 1.0f : -1.0f;
                velocity.x -= vortexDir * 0.4f;
                velocity.y += 0.2f;
            }

            // Direct flow around the car nose
            float distToCenter = std::abs(point.x) / (frontWingWidth * 0.5f);
            if (distToCenter > 0.7f) {
                // Flow around side of nose
                float sideForce = 0.3f * (distToCenter - 0.7f) / 0.3f;
                velocity.x += (point.x > 0) ? sideForce : -sideForce;
            }
        }
    }

    // Apply rear wing aerodynamic effect
    void applyRearWingEffect(const glm::vec3& point, glm::vec3& velocity) {
        // Rear wing position
        float rearWingZ = carLength * 0.4f;
        float rearWingY = carHeight * 0.8f;
        float rearWingWidth = carWidth * 0.8f;
        float rearWingHeight = carHeight * 0.2f;

        // Check if point is near the rear wing
        if (point.z > rearWingZ - carLength * 0.1f &&
            point.z < rearWingZ + carLength * 0.2f &&
            point.y > rearWingY - rearWingHeight &&
            point.y < rearWingY + rearWingHeight &&
            std::abs(point.x) < rearWingWidth * 0.5f) {

            // Downward force (rear wing generates downforce)
            velocity.y -= 0.8f;

            // Accelerate flow over wing
            velocity.z += 0.5f;

            // Create wing-tip vortices at the edges
            if (std::abs(point.x) > rearWingWidth * 0.4f) {
                float vortexDir = point.x > 0 ? 1.0f : -1.0f;
                velocity.x -= vortexDir * 0.3f;
                velocity.y += 0.2f;
            }

            // Create turbulence behind the wing
            if (point.z > rearWingZ) {
                // More chaotic movement behind the wing
                velocity.y += (rand() % 100) / 500.0f - 0.1f;
                velocity.x += (rand() % 100) / 500.0f - 0.1f;
            }
        }
    }

    // Apply floor aerodynamic effect
    void applyFloorEffect(const glm::vec3& point, glm::vec3& velocity) {
        // Check if point is under the car (ground effect area)
        if (point.y < carHeight * 0.3f &&
            point.y > 0.0f &&
            point.z > -carLength * 0.4f &&
            point.z < carLength * 0.3f &&
            std::abs(point.x) < carWidth * 0.4f) {

            // Venturi effect - accelerate air under the car
            velocity.z += 0.4f;

            // Ground effect pulls car down
            velocity.y -= 0.1f;

            // For diffuser at the rear, create expanding flow pattern
            if (point.z > 0.0f) {
                // Diffuser expands the flow based on position
                float diffuserStrength = 0.5f * (point.z / carLength);

                // Upward component at diffuser exit
                velocity.y += diffuserStrength;

                // Outward component creating wider wake
                float sideForce = diffuserStrength * 0.7f;
                velocity.x += (point.x > 0) ? sideForce : -sideForce;
            }
        }
    }

    // Calculate pressure at a given point for visualization
    float calculatePressureAtPoint(const glm::vec3& point) {
        // Base pressure
        float pressure = 1.0f;

        // Calculate velocity magnitude at this point
        glm::vec3 vel = calculateVelocityAtPoint(point);
        float speed = glm::length(vel);

        // Apply Bernoulli's principle: higher velocity = lower pressure
        pressure = 2.0f / (0.5f * speed * speed + 0.5f);

        // Additional pressure effects for specific areas
        // Lower pressure above the car
        if (point.y > carHeight &&
            point.z > -carLength * 0.5f &&
            point.z < carLength * 0.5f &&
            std::abs(point.x) < carWidth * 0.5f) {
            pressure *= 0.7f;
        }

        // Very low pressure under the car (ground effect)
        if (point.y < carHeight * 0.3f &&
            point.y > 0.0f &&
            point.z > -carLength * 0.4f &&
            point.z < carLength * 0.3f &&
            std::abs(point.x) < carWidth * 0.4f) {
            pressure *= 0.4f;
        }

        return pressure;
    }

    // Calculate color for a point based on position and pressure
    glm::vec3 calculateColorForPoint(const glm::vec3& point, float pressure, bool isPastDRS) {
        // Check if the point is past the DRS/rear wing position
        bool pastDRS = point.z > drsPositionZ;

        // Base color determined by pressure:
        // - High pressure (low speed): red/orange
        // - Low pressure (high speed): blue/purple

        if (pastDRS) {
            // Points past DRS - highlight with more vibrant colors
            // Low pressure (high speed) areas: purple/magenta
            if (pressure < 0.6f) {
                return glm::vec3(
                    0.6f + (0.4f * (1.0f - pressure)),  // Red component
                    0.1f,                                // Green component
                    0.8f                                 // Blue component
                );
            }
            // High pressure (low speed) areas: bright orange/yellow
            else {
                return glm::vec3(
                    0.9f,                                // Red component
                    0.4f + (0.4f * (1.0f - pressure)),   // Green component
                    0.0f                                 // Blue component
                );
            }
        }
        else {
            // Points before DRS
            // Use a cooler color scheme (blues to cyans)
            if (pressure < 0.5f) {
                // Very low pressure: deep blue
                return glm::vec3(0.1f, 0.2f, 0.9f);
            }
            else if (pressure < 0.8f) {
                // Medium pressure: cyan/teal
                return glm::vec3(0.1f, 0.6f, 0.8f);
            }
            else {
                // High pressure: light blue
                return glm::vec3(0.2f, 0.5f, 0.7f);
            }
        }
    }

    // Check if a point is valid (not too far from car, not below ground, etc.)
    bool isPointValid(const glm::vec3& point) {
        // Check if point is within valid bounds
        if (point.y < -0.1f) return false;  // Below ground

        // Check distance from car center
        float distToCenter = glm::length(glm::vec3(point.x, point.y - carHeight * 0.5f, point.z));
        if (distToCenter > carLength * 3.0f) return false;  // Too far from car

        return true;
    }

    // Draw the flow visualization
    void draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
        // Use shader
        shader.use();

        // Set view and projection matrices
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        shader.setMat4("model", glm::mat4(1.0f));

        // Enable line smoothing for better visuals
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(2.0f);  // Thicker lines for better visibility

        // Enable blending for semi-transparent lines
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Bind VAO
        glBindVertexArray(VAO);

        // Prepare buffer data
        std::vector<float> lineData;
        lineData.reserve(6 * maxPoints * numLines);  // Pre-allocate space

        for (const auto& line : flowLines) {
            if (line.points.size() < 2) continue;  // Skip lines with insufficient points

            // Add all points of this line
            for (size_t i = 0; i < line.points.size(); i++) {
                // Position
                lineData.push_back(line.points[i].x);
                lineData.push_back(line.points[i].y);
                lineData.push_back(line.points[i].z);

                // Color
                lineData.push_back(line.colors[i].r);
                lineData.push_back(line.colors[i].g);
                lineData.push_back(line.colors[i].b);
            }
        }

        // Upload line data to GPU
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, lineData.size() * sizeof(float), lineData.data());

        // Set alpha based on line life
        shader.setFloat("lineAlpha", 0.8f);  // Constant alpha for now

        // Draw all lines
        int offset = 0;
        for (const auto& line : flowLines) {
            if (line.points.size() < 2) continue;  // Skip lines with insufficient points

            // Calculate alpha based on line life
            float alpha = line.life / line.maxLife;
            shader.setFloat("lineAlpha", alpha * 0.8f);

            // Draw this line as a line strip
            glDrawArrays(GL_LINE_STRIP, offset, line.points.size());
            offset += line.points.size();
        }

        // Cleanup
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_BLEND);
        glBindVertexArray(0);
    }

    // Clean up resources
    void cleanup() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

private:
    std::vector<FlowLine> flowLines;
    unsigned int VAO, VBO;
    float carLength, carWidth, carHeight;
    float drsPositionZ;
    int numLines;
    int maxPoints;
};

#endif // FLOW_LINES_VISUALIZATION_H