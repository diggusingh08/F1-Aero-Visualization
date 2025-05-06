#ifndef TELEMETRY_VISUALIZATION_H
#define TELEMETRY_VISUALIZATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <deque>
#include <algorithm>
#include "Shader.h"

// Structure to hold telemetry data points
struct TelemetryDataPoint {
    float time;              // Time stamp
    float speed;             // Speed in km/h
    float downforce;         // Downforce in kg
    float drag;              // Drag force in kg
    float frontBalance;      // Front downforce balance (0.0-1.0)
    float lateralBalance;    // Lateral balance (negative = left, positive = right)
    float temperatureFront;  // Front tire temperature (degrees C)
    float temperatureRear;   // Rear tire temperature (degrees C)
    float drsActive;         // DRS active status (0.0 or 1.0)
};

class TelemetryVisualization {
public:
    TelemetryVisualization(int maxDataPoints = 500) {
        m_maxDataPoints = maxDataPoints;
        m_currentTime = 0.0f;
        m_displayWidth = 800.0f;
        m_displayHeight = 200.0f;
        m_displayMargin = 20.0f;
        m_horizontalTimeScale = 10.0f;  // 10 seconds displayed horizontally

        // Initialize metrics for each telemetry type
        m_speedMetric.color = glm::vec3(0.0f, 0.7f, 1.0f);
        m_speedMetric.min = 0.0f;
        m_speedMetric.max = 350.0f;
        m_speedMetric.visible = true;
        m_speedMetric.name = "Speed (km/h)";

        m_downforceMetric.color = glm::vec3(0.0f, 1.0f, 0.0f);
        m_downforceMetric.min = 0.0f;
        m_downforceMetric.max = 5000.0f;
        m_downforceMetric.visible = true;
        m_downforceMetric.name = "Downforce (kg)";

        m_dragMetric.color = glm::vec3(1.0f, 0.0f, 0.0f);
        m_dragMetric.min = 0.0f;
        m_dragMetric.max = 2000.0f;
        m_dragMetric.visible = true;
        m_dragMetric.name = "Drag (kg)";

        m_frontBalanceMetric.color = glm::vec3(1.0f, 0.7f, 0.0f);
        m_frontBalanceMetric.min = 0.0f;
        m_frontBalanceMetric.max = 1.0f;
        m_frontBalanceMetric.visible = false;
        m_frontBalanceMetric.name = "Front Balance";

        m_temperatureMetric.color = glm::vec3(1.0f, 0.2f, 0.7f);
        m_temperatureMetric.min = 20.0f;
        m_temperatureMetric.max = 120.0f;
        m_temperatureMetric.visible = false;
        m_temperatureMetric.name = "Tire Temp (°C)";

        m_drsMetric.color = glm::vec3(0.9f, 0.9f, 0.0f);
        m_drsMetric.min = 0.0f;
        m_drsMetric.max = 1.0f;
        m_drsMetric.visible = true;
        m_drsMetric.name = "DRS Active";

        setupBuffers();
    }

    ~TelemetryVisualization() {
        cleanup();
    }

    // Add new telemetry data point
    void addDataPoint(const TelemetryDataPoint& dataPoint) {
        // Add the data point to the queue
        m_telemetryData.push_back(dataPoint);

        // Remove oldest data point if we exceed max capacity
        if (m_telemetryData.size() > m_maxDataPoints) {
            m_telemetryData.pop_front();
        }

        // Update current time
        m_currentTime = dataPoint.time;

        // Update display data for rendering
        updateDisplayData();
    }

    // Calculate downforce and drag based on current aerodynamic conditions
    TelemetryDataPoint calculateAerodynamics(float speed, bool drsActive, float frontRideHeight, float rearRideHeight) {
        TelemetryDataPoint data;
        data.time = m_currentTime;
        data.speed = speed;
        data.drsActive = drsActive ? 1.0f : 0.0f;

        // Base coefficients
        float baseCd = 0.7f;  // Base drag coefficient
        float baseCl = 3.0f;  // Base lift coefficient (negative for downforce)

        // Modify coefficients based on DRS state
        if (drsActive) {
            baseCd *= 0.75f;  // DRS reduces drag by about 25%
            baseCl *= 0.85f;  // DRS also reduces downforce
        }

        // Modify coefficients based on ride height (ground effect)
        float groundEffectMultiplier = calculateGroundEffect(frontRideHeight, rearRideHeight);
        baseCl *= groundEffectMultiplier;

        // Calculate forces based on speed, air density, and frontal area
        float airDensity = 1.225f;  // kg/m^3 at sea level
        float frontalArea = 1.5f;   // m^2 for an F1 car
        float dynamicPressure = 0.5f * airDensity * powf(speed / 3.6f, 2);  // Convert km/h to m/s

        data.downforce = baseCl * dynamicPressure * frontalArea;
        data.drag = baseCd * dynamicPressure * frontalArea;

        // Calculate balance - front typically has 40-45% of downforce
        data.frontBalance = 0.42f + (rearRideHeight - frontRideHeight) * 0.05f;

        // Lateral balance - assume symmetrical for now
        data.lateralBalance = 0.0f;

        // Simulate tire temperatures based on downforce and speed
        data.temperatureFront = 20.0f + (data.speed / 350.0f) * 80.0f +
            (data.downforce / 5000.0f) * data.frontBalance * 20.0f;
        data.temperatureRear = 20.0f + (data.speed / 350.0f) * 80.0f +
            (data.downforce / 5000.0f) * (1.0f - data.frontBalance) * 20.0f;

        return data;
    }

    // Draw telemetry visualization
    void draw(Shader& shader, float screenWidth, float screenHeight) {
        if (m_telemetryData.empty()) return;

        shader.use();

        // Set up orthographic projection for 2D drawing
        glm::mat4 projection = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
        shader.setMat4("projection", projection);
        shader.setMat4("view", glm::mat4(1.0f));
        shader.setMat4("model", glm::mat4(1.0f));

        // Position the telemetry display in the bottom right corner
        float xPos = screenWidth - m_displayWidth - m_displayMargin;
        float yPos = m_displayMargin;

        // Draw background
        drawBackground(shader, xPos, yPos);

        // Draw grid
        drawGrid(shader, xPos, yPos);

        // Draw data lines
        drawDataLines(shader, xPos, yPos);

        // Draw labels
        drawLabels(shader, xPos, yPos);
    }

    // Set which metrics to display
    void setMetricVisibility(bool speed, bool downforce, bool drag, bool frontBalance, bool temperature, bool drs) {
        m_speedMetric.visible = speed;
        m_downforceMetric.visible = downforce;
        m_dragMetric.visible = drag;
        m_frontBalanceMetric.visible = frontBalance;
        m_temperatureMetric.visible = temperature;
        m_drsMetric.visible = drs;

        updateDisplayData();
    }

    // Set time scale (how many seconds of data to show)
    void setTimeScale(float seconds) {
        m_horizontalTimeScale = std::max(1.0f, seconds);
        updateDisplayData();
    }

    // Set display dimensions
    void setDisplaySize(float width, float height) {
        m_displayWidth = width;
        m_displayHeight = height;
        updateDisplayData();
    }

    // Cleanup resources
    void cleanup() {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_colorVBO);
    }

private:
    // Structure to hold metric display properties
    struct MetricInfo {
        glm::vec3 color;
        float min;
        float max;
        bool visible;
        std::string name;
    };

    // Calculate ground effect multiplier based on ride height
    float calculateGroundEffect(float frontHeight, float rearHeight) {
        // Ground effect is strongest at low ride heights
        // Modern F1 cars generate more downforce the closer they are to the ground
        float avgHeight = (frontHeight + rearHeight) / 2.0f;

        // Exponential relationship between height and ground effect
        float baseMultiplier = 1.0f + 0.5f * expf(-avgHeight * 2.0f);

        // Penalize excessive rake (difference between front and rear)
        float rakePenalty = 0.0f;
        if (rearHeight - frontHeight > 0.1f) {
            rakePenalty = (rearHeight - frontHeight - 0.1f) * 0.5f;
        }

        return baseMultiplier - rakePenalty;
    }

    // Set up OpenGL buffers
    void setupBuffers() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_colorVBO);

        glBindVertexArray(m_VAO);

        // Position buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_maxDataPoints * 6, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // Color buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_maxDataPoints * 6, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Update vertex and color data for display
    void updateDisplayData() {
        if (m_telemetryData.empty()) return;

        m_displayVertices.clear();
        m_displayColors.clear();

        // Calculate the time window to display
        float startTime = m_currentTime - m_horizontalTimeScale;

        // For each visible metric, add line segments
        if (m_speedMetric.visible) {
            addMetricLineData(startTime, m_speedMetric,
                [](const TelemetryDataPoint& dp) { return dp.speed; });
        }

        if (m_downforceMetric.visible) {
            addMetricLineData(startTime, m_downforceMetric,
                [](const TelemetryDataPoint& dp) { return dp.downforce; });
        }

        if (m_dragMetric.visible) {
            addMetricLineData(startTime, m_dragMetric,
                [](const TelemetryDataPoint& dp) { return dp.drag; });
        }

        if (m_frontBalanceMetric.visible) {
            addMetricLineData(startTime, m_frontBalanceMetric,
                [](const TelemetryDataPoint& dp) { return dp.frontBalance; });
        }

        if (m_temperatureMetric.visible) {
            // Add front temperature line
            MetricInfo frontTempMetric = m_temperatureMetric;
            frontTempMetric.color = glm::vec3(1.0f, 0.2f, 0.2f); // Red for front
            addMetricLineData(startTime, frontTempMetric,
                [](const TelemetryDataPoint& dp) { return dp.temperatureFront; });

            // Add rear temperature line
            MetricInfo rearTempMetric = m_temperatureMetric;
            rearTempMetric.color = glm::vec3(0.2f, 0.2f, 1.0f); // Blue for rear
            addMetricLineData(startTime, rearTempMetric,
                [](const TelemetryDataPoint& dp) { return dp.temperatureRear; });
        }

        if (m_drsMetric.visible) {
            addMetricLineData(startTime, m_drsMetric,
                [](const TelemetryDataPoint& dp) { return dp.drsActive; });
        }

        // Update OpenGL buffers
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_displayVertices.size() * sizeof(glm::vec3),
            m_displayVertices.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, m_colorVBO);
        glBufferData(GL_ARRAY_BUFFER, m_displayColors.size() * sizeof(glm::vec3),
            m_displayColors.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Add line data for a specific metric
    template<typename ValueGetter>
    void addMetricLineData(float startTime, const MetricInfo& metric, ValueGetter getValue) {
        // Find the first data point after the start time
        auto startIter = std::find_if(m_telemetryData.begin(), m_telemetryData.end(),
            [startTime](const TelemetryDataPoint& dp) { return dp.time >= startTime; });

        // If no data points in range, return
        if (startIter == m_telemetryData.end()) return;

        // Include one data point before start time if available
        if (startIter != m_telemetryData.begin()) {
            startIter--;
        }

        // Add line segments connecting all data points
        TelemetryDataPoint prevDP = *startIter;
        float prevX = mapTimeToX(prevDP.time, startTime);
        float prevY = mapValueToY(getValue(prevDP), metric.min, metric.max);

        for (auto iter = std::next(startIter); iter != m_telemetryData.end(); ++iter) {
            const TelemetryDataPoint& dp = *iter;
            float x = mapTimeToX(dp.time, startTime);
            float y = mapValueToY(getValue(dp), metric.min, metric.max);

            // Add line segment
            m_displayVertices.push_back(glm::vec3(prevX, prevY, 0.0f));
            m_displayVertices.push_back(glm::vec3(x, y, 0.0f));

            m_displayColors.push_back(metric.color);
            m_displayColors.push_back(metric.color);

            prevX = x;
            prevY = y;
        }
    }

    // Map time value to X coordinate
    float mapTimeToX(float time, float startTime) {
        float normalizedTime = (time - startTime) / m_horizontalTimeScale;
        return normalizedTime * m_displayWidth;
    }

    // Map data value to Y coordinate
    float mapValueToY(float value, float minValue, float maxValue) {
        float normalizedValue = (value - minValue) / (maxValue - minValue);
        return normalizedValue * m_displayHeight;
    }

    // Draw the background panel
    void drawBackground(Shader& shader, float xPos, float yPos) {
        // Implementation for drawing background would go here
        // This could use a separate quad renderer or immediate mode
    }

    // Draw grid lines
    void drawGrid(Shader& shader, float xPos, float yPos) {
        // Implementation for drawing grid would go here
    }

    // Draw data lines
    void drawDataLines(Shader& shader, float xPos, float yPos) {
        // Set model matrix to position the graph
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(xPos, yPos, 0.0f));
        shader.setMat4("model", model);

        glBindVertexArray(m_VAO);

        // Enable line smoothing
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(2.0f);

        // Draw lines
        glDrawArrays(GL_LINES, 0, m_displayVertices.size());

        glDisable(GL_LINE_SMOOTH);
        glBindVertexArray(0);
    }

    // Draw labels for metrics
    void drawLabels(Shader& shader, float xPos, float yPos) {
        // Implementation for drawing labels would go here
        // This would typically use a text renderer
    }

private:
    // OpenGL objects
    GLuint m_VAO, m_VBO, m_colorVBO;

    // Telemetry data
    std::deque<TelemetryDataPoint> m_telemetryData;
    int m_maxDataPoints;
    float m_currentTime;

    // Display properties
    float m_displayWidth;
    float m_displayHeight;
    float m_displayMargin;
    float m_horizontalTimeScale;

    // Metric information
    MetricInfo m_speedMetric;
    MetricInfo m_downforceMetric;
    MetricInfo m_dragMetric;
    MetricInfo m_frontBalanceMetric;
    MetricInfo m_temperatureMetric;
    MetricInfo m_drsMetric;

    // Vertices and colors for rendering
    std::vector<glm::vec3> m_displayVertices;
    std::vector<glm::vec3> m_displayColors;
};

#endif // TELEMETRY_VISUALIZATION_H