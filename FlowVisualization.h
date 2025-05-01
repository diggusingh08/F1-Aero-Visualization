#ifndef FLOW_VISUALIZATION_H
#define FLOW_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <random>
#include "Shader.h"

// Structure to hold particle data
struct FlowParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float life;
    float size;
};

class FlowVisualization {
public:
    // Constructor
    FlowVisualization(int numParticles, float carLength, float carWidth, float carHeight) {
        init(numParticles, carLength, carWidth, carHeight);
    }

    // Initialize the flow system
    void init(int numParticles, float carLength, float carWidth, float carHeight) {
        // Store car dimensions for flow calculations
        this->carLength = carLength;
        this->carWidth = carWidth;
        this->carHeight = carHeight;

        // Initialize the particles
        particles.resize(numParticles);

        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> xDist(-carLength, carLength * 2);
        std::uniform_real_distribution<float> yDist(0.0f, carHeight * 1.5f);
        std::uniform_real_distribution<float> zDist(-carWidth, carWidth);
        std::uniform_real_distribution<float> lifeDist(0.5f, 2.0f);
        std::uniform_real_distribution<float> sizeDist(0.01f, 0.03f);

        // Initialize particles with random positions
        for (auto& particle : particles) {
            resetParticle(particle, true, gen, xDist, yDist, zDist, lifeDist, sizeDist);
        }

        // Create and setup buffers
        setupBuffers();
    }

    // Reset a particle
    void resetParticle(FlowParticle& particle, bool randomPos,
        std::mt19937& gen,
        std::uniform_real_distribution<float>& xDist,
        std::uniform_real_distribution<float>& yDist,
        std::uniform_real_distribution<float>& zDist,
        std::uniform_real_distribution<float>& lifeDist,
        std::uniform_real_distribution<float>& sizeDist) {

        if (randomPos) {
            // Random position around the car
            particle.position = glm::vec3(xDist(gen), yDist(gen), zDist(gen));

            // Start with particles in front of car for better visualization
            if (particle.position.x > carLength * 0.5f)
                particle.position.x = -carLength;
        }
        else {
            // Reset to start position
            particle.position = glm::vec3(-carLength,
                0.1f + sizeDist(gen) * carHeight,
                (zDist(gen) / 2.0f));
        }

        // Initial velocity (flowing from front to back of car)
        particle.velocity = glm::vec3(1.0f + (randomFloat(gen) * 0.3f),
            randomFloat(gen) * 0.1f,
            randomFloat(gen) * 0.1f);

        // Color varies from blue (cool) to red (hot)
        particle.color = glm::vec3(0.1f, 0.4f, 0.9f); // Default blue

        // Life determines how long the particle exists
        particle.life = lifeDist(gen);

        // Size of the particle
        particle.size = sizeDist(gen);
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

        // Setup the data for a point sprite
        std::vector<float> particleVertices = {
            // Position     // Color
            0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
        };

        // Bind vertex array
        glBindVertexArray(VAO);

        // Bind and set vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, particleVertices.size() * sizeof(float),
            particleVertices.data(), GL_STATIC_DRAW);

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

    // Update particle positions based on aerodynamic simulation
    void update(float deltaTime) {
        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> xDist(-carLength, carLength * 2);
        std::uniform_real_distribution<float> yDist(0.0f, carHeight * 1.5f);
        std::uniform_real_distribution<float> zDist(-carWidth, carWidth);
        std::uniform_real_distribution<float> lifeDist(0.5f, 2.0f);
        std::uniform_real_distribution<float> sizeDist(0.01f, 0.03f);

        for (auto& particle : particles) {
            // Decrease life
            particle.life -= deltaTime;

            if (particle.life > 0.0f) {
                // Update position based on velocity
                particle.position += particle.velocity * deltaTime * 2.0f;

                // Simplified aerodynamic forces
                // 1. Flow around the car body
                applyCarBodyForces(particle);

                // 2. Updraft from rear wing
                applyRearWingForces(particle);

                // 3. Front wing effects
                applyFrontWingForces(particle);

                // 4. Floor and diffuser effects
                applyFloorForces(particle);

                // Update color based on velocity (faster = more red)
                float speed = glm::length(particle.velocity);
                particle.color = glm::vec3(
                    glm::clamp(speed * 0.5f, 0.0f, 1.0f),  // Red increases with speed
                    glm::clamp(0.4f - speed * 0.1f, 0.0f, 0.4f),  // Green decreases
                    glm::clamp(0.9f - speed * 0.3f, 0.0f, 0.9f)   // Blue decreases
                );
            }
            else {
                // Reset particle if it's dead
                resetParticle(particle, false, gen, xDist, yDist, zDist, lifeDist, sizeDist);
            }
        }
    }

    // Apply forces from car body
    void applyCarBodyForces(FlowParticle& particle) {
        // Simplified car body as a box
        float carX = 0.0f; // Car centered at origin

        // Check if particle is near the car body
        if (particle.position.x > carX - carLength * 0.5f &&
            particle.position.x < carX + carLength * 0.5f &&
            particle.position.y < carHeight &&
            std::abs(particle.position.z) < carWidth * 0.5f) {

            // Compute distance to car surface
            float distToSurface = 0.1f; // Simplification

            // Direction away from car body (simplified)
            glm::vec3 awayFromCar = glm::normalize(glm::vec3(
                0.0f,
                particle.position.y < carHeight * 0.5f ? -1.0f : 1.0f,
                particle.position.z
            ));

            // Apply force away from car with strength decreasing with distance
            float forceMagnitude = 1.0f / (distToSurface + 0.1f);
            particle.velocity += awayFromCar * forceMagnitude * 0.01f;

            // Ensure particle doesn't get stuck inside car
            if (particle.position.y < carHeight * 0.1f) {
                particle.position.y = carHeight * 0.1f;
                particle.velocity.y = std::abs(particle.velocity.y) * 0.2f;
            }
        }
    }

    // Apply forces from rear wing
    void applyRearWingForces(FlowParticle& particle) {
        // Rear wing position (simplified)
        float rearWingX = carLength * 0.4f;
        float rearWingY = carHeight * 0.8f;

        // Distance to rear wing
        float distToWing = glm::length(glm::vec2(
            particle.position.x - rearWingX,
            particle.position.y - rearWingY
        ));

        // Apply upward force if near the rear wing
        if (distToWing < carLength * 0.2f &&
            particle.position.x > rearWingX - carLength * 0.1f &&
            particle.position.x < rearWingX + carLength * 0.1f) {

            // Downward force (rear wing generates downforce)
            float forceMagnitude = 0.05f / (distToWing + 0.1f);
            particle.velocity.y -= forceMagnitude;

            // Create some turbulence behind the wing
            if (particle.position.x > rearWingX) {
                particle.velocity.y += (rand() % 100) / 1000.0f - 0.05f;
                particle.velocity.z += (rand() % 100) / 1000.0f - 0.05f;
            }
        }
    }

    // Apply forces from front wing
    void applyFrontWingForces(FlowParticle& particle) {
        // Front wing position (simplified)
        float frontWingX = -carLength * 0.4f;
        float frontWingY = carHeight * 0.2f;

        // Distance to front wing
        float distToWing = glm::length(glm::vec2(
            particle.position.x - frontWingX,
            particle.position.y - frontWingY
        ));

        // Apply downforce if near the front wing
        if (distToWing < carLength * 0.2f &&
            particle.position.x > frontWingX - carLength * 0.1f &&
            particle.position.x < frontWingX + carLength * 0.1f) {

            // Downward force
            float forceMagnitude = 0.03f / (distToWing + 0.1f);
            particle.velocity.y -= forceMagnitude;

            // Create some vortices at the wing tips
            if (std::abs(particle.position.z) > carWidth * 0.4f &&
                std::abs(particle.position.z) < carWidth * 0.6f) {

                // Create wingtip vortices
                float vortexStrength = 0.02f;
                float vortexDir = particle.position.z > 0 ? 1.0f : -1.0f;
                particle.velocity.z -= vortexDir * vortexStrength;
                particle.velocity.y += vortexStrength * 0.5f;
            }
        }
    }

    // Apply forces from floor and diffuser
    void applyFloorForces(FlowParticle& particle) {
        // Check if particle is under the car
        if (particle.position.y < carHeight * 0.3f &&
            particle.position.y > 0.0f &&
            particle.position.x > -carLength * 0.4f &&
            particle.position.x < carLength * 0.3f &&
            std::abs(particle.position.z) < carWidth * 0.4f) {

            // Accelerate air under the car (ground effect)
            particle.velocity.x += 0.02f;

            // For diffuser, create an expanding flow at the rear
            if (particle.position.x > 0.0f) {
                // Diffuser expands the flow
                float diffuserStrength = 0.03f * (particle.position.x / carLength);
                particle.velocity.y += diffuserStrength;
                particle.velocity.z += (particle.position.z > 0) ? diffuserStrength : -diffuserStrength;
            }
        }
    }

    // Draw the flow visualization
    void draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
        // Enable blending for semi-transparent particles
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Enable point size
        glEnable(GL_PROGRAM_POINT_SIZE);

        // Use shader
        shader.use();

        // Set view and projection matrices
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // Draw each particle
        glBindVertexArray(VAO);
        for (const auto& particle : particles) {
            // Set particle properties
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, particle.position);
            model = glm::scale(model, glm::vec3(particle.size));

            shader.setMat4("model", model);
            shader.setVec3("particleColor", particle.color);
            shader.setFloat("particleAlpha", particle.life * 0.5f); // Fade out as life decreases

            // Draw point
            glDrawArrays(GL_POINTS, 0, 1);
        }

        // Disable states
        glDisable(GL_BLEND);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glBindVertexArray(0);
    }

    // Clean up resources
    void cleanup() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

private:
    std::vector<FlowParticle> particles;
    unsigned int VAO, VBO;
    float carLength, carWidth, carHeight;
};

#endif // FLOW_VISUALIZATION_H