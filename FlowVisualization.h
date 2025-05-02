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
    bool passedDRS;     // Flag to track if particle has passed DRS/rear wing
    float pressure;     // Store pressure value for visualization
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

        // Define DRS position for pressure change detection
        drsPositionZ = carLength * 0.4f;  // Position of rear wing/DRS (now on Z-axis)

        // Initialize the particles
        particles.resize(numParticles);

        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());

        // Adjusted distributions focusing on the front of the car
        // Most particles start in front of the car now
// Adjusted distributions for side view of the car
        // Most particles start to the side of the car now
        std::uniform_real_distribution<float> zDist(-carLength * 2.5f, -carLength * 0.5f); // Now Z is the front-back axis
        std::uniform_real_distribution<float> yDist(0.0f, carHeight * 2.0f);  // Y still up
        std::uniform_real_distribution<float> xDist(-carWidth * 1.5f, carWidth * 1.5f);  // X is now the side axis

        // Longer life for better flow visualization
        std::uniform_real_distribution<float> lifeDist(2.0f, 6.0f);
        std::uniform_real_distribution<float> sizeDist(0.03f, 0.08f);

        // Initialize particles with positions in front of the car
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

        // Initial position - always start in front of the car
        if (randomPos) {
            // Random position in front of the car
            particle.position = glm::vec3(xDist(gen), yDist(gen), zDist(gen));
        }
        else {
            // Reset to a specific starting position
                        // Distribute along the side of the car for better visualization
            float sideOffset = carWidth * 0.8f;
            float heightVariation = carHeight * 1.0f;
            float lengthVariation = carLength * 0.8f;

            particle.position = glm::vec3(
                -sideOffset - (rand() % 100) / 200.0f,  // Slightly randomize X position (now side of car)
                (rand() % 100) / 100.0f * heightVariation,  // Vary height
                ((rand() % 200) - 100) / 100.0f * lengthVariation  // Vary position along length (Z-axis now)
            );
        }


// Initial velocity (directional flow from side of car)
        // Higher initial speed for better visualization
        particle.velocity = glm::vec3(1.5f + (randomFloat(gen) * 0.5f),  // Now moving along X-axis (side to side)
            randomFloat(gen) * 0.2f,                                    // Small vertical variation
            randomFloat(gen) * 0.2f);                                   // Small variation along Z-axis

        // Starting color - blue to indicate cool, low pressure areas
        particle.color = glm::vec3(0.1f, 0.4f, 0.9f);

        // Reset passed DRS flag
        particle.passedDRS = false;

        // Initial pressure (normal atmosphere)
        particle.pressure = 1.0f;

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
        std::uniform_real_distribution<float> xDist(-carLength * 2.5f, -carLength * 0.5f);
        std::uniform_real_distribution<float> yDist(0.0f, carHeight * 2.0f);
        std::uniform_real_distribution<float> zDist(-carWidth * 1.5f, carWidth * 1.5f);
        std::uniform_real_distribution<float> lifeDist(2.0f, 6.0f);
        std::uniform_real_distribution<float> sizeDist(0.03f, 0.08f);

        for (auto& particle : particles) {
            // Decrease life
            particle.life -= deltaTime;

            if (particle.life > 0.0f) {
                // Update position based on velocity
                particle.position += particle.velocity * deltaTime * 2.0f;

                // Apply all aerodynamic forces
                applyAerodynamicForces(particle, deltaTime);

                // Check if particle has passed the DRS/rear wing
                if (!particle.passedDRS && particle.position.z > drsPositionZ) {
                    particle.passedDRS = true;
                }

                // Update color based on pressure and DRS passage
                updateParticleColor(particle);
            }
            else {
                // Reset particle if it's dead
                resetParticle(particle, false, gen, xDist, yDist, zDist, lifeDist, sizeDist);
            }
        }
    }

    // Apply all aerodynamic forces to a particle
    void applyAerodynamicForces(FlowParticle& particle, float deltaTime) {
        // Car body forces
        applyCarBodyForces(particle);

        // Rear wing / DRS forces
        applyRearWingForces(particle);

        // Front wing effects
        applyFrontWingForces(particle);

        // Floor and diffuser effects
        applyFloorForces(particle);

        // Calculate pressure based on velocity and position
        calculatePressure(particle);
    }

    // Calculate pressure for visualization
    void calculatePressure(FlowParticle& particle) {
        // Simplified Bernoulli's principle: 
        // Higher velocity = lower pressure
        float speed = glm::length(particle.velocity);

        // Inverse relationship between speed and pressure
        // Normalize to a reasonable range
        particle.pressure = 2.0f / (0.5f * speed * speed + 0.5f);

        // Additional pressure effects for specific areas
        // Lower pressure above the car
        if (particle.position.y > carHeight &&
            particle.position.x > -carLength * 0.5f &&
            particle.position.x < carLength * 0.5f) {
            particle.pressure *= 0.7f;
        }

        // Very low pressure under the car (ground effect)
        if (particle.position.y < carHeight * 0.3f &&
            particle.position.y > 0.0f &&
            particle.position.x > -carLength * 0.4f &&
            particle.position.x < carLength * 0.3f) {
            particle.pressure *= 0.4f;
        }
    }

    // Update particle color based on pressure and DRS passage
    void updateParticleColor(FlowParticle& particle) {
        // Base color determined by pressure:
        // - High pressure (low speed): red/orange
        // - Low pressure (high speed): blue/purple

        if (particle.passedDRS) {
            // Particles that have passed DRS - highlight with more vibrant colors
            // Low pressure (high speed) areas: purple/magenta
            if (particle.pressure < 0.6f) {
                particle.color = glm::vec3(
                    0.6f + (0.4f * (1.0f - particle.pressure)),  // Red component
                    0.1f,                                        // Green component
                    0.8f                                         // Blue component
                );
            }
            // High pressure (low speed) areas: bright orange/yellow
            else {
                particle.color = glm::vec3(
                    0.9f,                                        // Red component
                    0.4f + (0.4f * (1.0f - particle.pressure)),  // Green component
                    0.0f                                         // Blue component
                );
            }
        }
        else {
            // Particles that haven't passed DRS yet
            // Use a cooler color scheme (blues to cyans)
            if (particle.pressure < 0.5f) {
                // Very low pressure: deep blue
                particle.color = glm::vec3(0.1f, 0.2f, 0.9f);
            }
            else if (particle.pressure < 0.8f) {
                // Medium pressure: cyan/teal
                particle.color = glm::vec3(0.1f, 0.6f, 0.8f);
            }
            else {
                // High pressure: light blue
                particle.color = glm::vec3(0.2f, 0.5f, 0.7f);
            }
        }
    }

    // Apply forces from car body
    void applyCarBodyForces(FlowParticle& particle) {
        // Simplified car body as a box
        float carZ = 0.0f; // Car centered at origin

        // Check if particle is near the car body (with updated orientation)
        if (particle.position.z > carZ - carLength * 0.5f &&
            particle.position.z < carZ + carLength * 0.5f &&
            particle.position.y < carHeight &&
            std::abs(particle.position.x) < carWidth * 0.5f) {

            // Compute distance to car surface (simplified)
            float distToSurface = 0.1f;

            // Direction away from car body (improved for better flow visualization)
// Direction away from car body (improved for better flow visualization)
            glm::vec3 awayFromCar;

            // Top surface - upward flow
            if (particle.position.y > carHeight * 0.8f) {
                awayFromCar = glm::normalize(glm::vec3(0.5f, 1.0f, particle.position.x * 0.2f));
            }
            // Bottom surface - downward flow with ground effect
            else if (particle.position.y < carHeight * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(1.0f, -0.2f, particle.position.x * 0.1f));
            }
            // Side surfaces - outward flow
            else if (std::abs(particle.position.x) > carWidth * 0.4f) {
                float sideDir = particle.position.x > 0 ? 1.0f : -1.0f;
                awayFromCar = glm::normalize(glm::vec3(sideDir, 0.1f, 0.5f));
            }
            // Front of car - flows around
            else if (particle.position.z < -carLength * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(
                    particle.position.x * 0.7f,
                    particle.position.y < carHeight * 0.5f ? -0.3f : 0.3f,
                    0.2f
                ));
            }
            // Rear of car - flows behind
            else if (particle.position.z > carLength * 0.3f) {
                awayFromCar = glm::normalize(glm::vec3(
                    particle.position.x * 0.3f,
                    particle.position.y < carHeight * 0.5f ? -0.2f : 0.2f,
                    1.0f
                ));
            }
            // Middle of car
            else {
                awayFromCar = glm::normalize(glm::vec3(
                    particle.position.x,
                    particle.position.y < carHeight * 0.5f ? -0.3f : 0.3f,
                    0.7f
                ));
            }
            // Apply force away from car with strength decreasing with distance
            float forceMagnitude = 1.0f / (distToSurface + 0.1f);
            particle.velocity += awayFromCar * forceMagnitude * 0.02f;

            // Ensure particle doesn't get stuck inside car
            if (particle.position.y < carHeight * 0.1f && particle.position.y > 0.0f) {
                particle.position.y = carHeight * 0.1f;
                particle.velocity.y *= 0.5f;
            }
        }
    }

    // Apply forces from rear wing (DRS area)
    void applyRearWingForces(FlowParticle& particle) {
        // Rear wing position (updated for new orientation)
        float rearWingZ = carLength * 0.4f;  // Now Z represents the length axis
        float rearWingY = carHeight * 0.8f;
        float rearWingX = 0.0f;
        float rearWingWidth = carWidth * 0.8f;
        float rearWingHeight = carHeight * 0.2f;

        // Check if particle is near the rear wing (updated for new orientation)
        if (particle.position.z > rearWingZ - carLength * 0.1f &&
            particle.position.z < rearWingZ + carLength * 0.2f &&
            particle.position.y > rearWingY - rearWingHeight &&
            particle.position.y < rearWingY + rearWingHeight &&
            std::abs(particle.position.x) < rearWingWidth * 0.5f) {

            // Downward force (rear wing generates downforce)
            float forceMagnitude = 0.08f;
            particle.velocity.y -= forceMagnitude;

            // Accelerate flow over wing
            particle.velocity.z += 0.05f;  // Now Z is the flow direction

            // Create wing-tip vortices at the edges
            if (std::abs(particle.position.x) > rearWingWidth * 0.4f) {
                float vortexDir = particle.position.x > 0 ? 1.0f : -1.0f;
                particle.velocity.x -= vortexDir * 0.03f;
                particle.velocity.y += 0.02f;
            }

            // Create turbulence behind the wing
            if (particle.position.z > rearWingZ) {
                // More chaotic movement behind the wing
                particle.velocity.y += (rand() % 100) / 500.0f - 0.1f;
                particle.velocity.x += (rand() % 100) / 500.0f - 0.1f;
            }
        }
    }

    // Apply forces from front wing
    void applyFrontWingForces(FlowParticle& particle) {
        // Front wing position
        float frontWingX = -carLength * 0.4f;
        float frontWingY = carHeight * 0.2f;
        float frontWingWidth = carWidth * 0.9f;

        // Apply forces from front wing
        void applyFrontWingForces(FlowParticle & particle) {
            // Front wing position (updated for new orientation)
            float frontWingZ = -carLength * 0.4f;  // Now Z is the length axis
            float frontWingY = carHeight * 0.2f;
            float frontWingWidth = carWidth * 0.9f;

            // Check if particle is near the front wing (updated for new orientation)
            if (particle.position.z > frontWingZ - carLength * 0.1f &&
                particle.position.z < frontWingZ + carLength * 0.1f &&
                particle.position.y < frontWingY + carHeight * 0.1f &&
                std::abs(particle.position.x) < frontWingWidth * 0.5f) {

                // Downforce
                float forceMagnitude = 0.04f;
                particle.velocity.y -= forceMagnitude;

                // Accelerate flow under the wing
                particle.velocity.z += 0.03f;  // Now Z is the flow direction

                // Create wing-tip vortices
                if (std::abs(particle.position.x) > frontWingWidth * 0.4f) {
                    float vortexDir = particle.position.x > 0 ? 1.0f : -1.0f;
                    particle.velocity.x -= vortexDir * 0.04f;
                    particle.velocity.y += 0.02f;

                    // Color change for vortices
                    particle.color = glm::vec3(0.3f, 0.5f, 0.9f);
                }

                // Direct flow around the car
                if (particle.position.y > frontWingY) {
                    // Flow over the top of the wing
                    particle.velocity.y += 0.02f;
                }
            }
    }

        // Apply forces from floor and diffuser
        void applyFloorForces(FlowParticle& particle) {
            // Check if particle is under the car (ground effect area)
            // Updated for new orientation - now Z is the length axis
            if (particle.position.y < carHeight * 0.3f &&
                particle.position.y > 0.0f &&
                particle.position.z > -carLength * 0.4f &&
                particle.position.z < carLength * 0.3f &&
                std::abs(particle.position.x) < carWidth * 0.4f) {

                // Venturi effect - accelerate air under the car
                particle.velocity.z += 0.04f;  // Now Z is the flow direction

                // Ground effect pulls car down
                particle.velocity.y -= 0.01f;

                // For diffuser at the rear, create expanding flow pattern
                if (particle.position.z > 0.0f) {
                    // Diffuser expands the flow based on position
                    float diffuserStrength = 0.05f * (particle.position.z / carLength);

                    // Upward component at diffuser exit
                    particle.velocity.y += diffuserStrength;

                    // Outward component creating wider wake
                    float sideForce = diffuserStrength * 0.7f;
                    particle.velocity.x += (particle.position.x > 0) ? sideForce : -sideForce;
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

            // Adjust alpha based on life and pressure
            float alpha = particle.life * 0.5f;

            // Make low pressure areas more visible
            if (particle.pressure < 0.6f) {
                alpha *= 1.2f;
            }

            shader.setFloat("particleAlpha", alpha);

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
    float drsPositionZ;  // Z position of DRS/rear wing for detection (used to be X) 
};

#endif // FLOW_VISUALIZATION_H