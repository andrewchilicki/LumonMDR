#pragma once

#include <fstream>
#include <iostream>
#include <optional>
#include <json.hpp>

struct PresetDisplaySettings
{
    // Preset sizing and offsets for UI
    float numberWindowBufferTop = 125.f;
    float numberWindowBufferBottom = 115.f;

    float headerImageScale = 0.25f;
    float headerImageOffsetX = 50.f;
    float headerBoxBufferX = 55.f;
    float headerBoxBufferY = 37.f;

    float binImageScale = 0.75f;
    float binOffset = 40.f;
    float binPercentBarOffset = 50.f;
    float fontSize = 29.f;
    float settingsFontScale = 0.3f;

    float lineGraphicsSpacing = 10.f;
    float lineThickness = 5.f;
};

// Helpers
inline std::optional<nlohmann::json> loadSettingsFromJson(const std::string& jsonPath)
{
    try {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open settings file: " << jsonPath << std::endl;
            return std::nullopt;
        }

        nlohmann::json json;
        file >> json;
        return json;
    } catch (const std::exception& e) {
        std::cerr << "Error loading json: " << e.what() << std::endl;
    }

    return std::nullopt;
}
inline void saveSettingsJson(const nlohmann::json& settingsJson, const std::string& jsonPath)
{
    try {
        std::ofstream file(jsonPath);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file for writing: " << jsonPath << std::endl;
            return;
        }

        file << settingsJson.dump(4);
        file.flush();
        file.close();
        std::cout << "Settings saved to disk." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving settings: " << e.what() << std::endl;
    }
}

// Numbers Panel settings
struct DisplaySettings
{
    float globalScale = 1.f;
    float imageScale = 0.75f;
    float gridSpacing = 200.f;

    float mouseScaleRadius = 100.f;
    float mouseScaleMultiplier = 2.f;
    float maxZoomScale = 0.8f;
    float minZoomScale = 0.2f;

    float noiseSpeed = 0.004f;
    float noiseScale = 1.f;
    float noiseScaleOffset = 15;

    float refinedToBinSpeed = 12.0f;

    std::string headerText = "@andrewchilicki";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DisplaySettings,
            globalScale,
            imageScale,
            gridSpacing,
            mouseScaleRadius,
            mouseScaleMultiplier,
            maxZoomScale,
            minZoomScale,
            noiseSpeed,
            noiseScale,
            noiseScaleOffset,
            refinedToBinSpeed,
            headerText
        );
};

struct ControlSettings
{
    float arrowSensitivity = 25.f; // Unused since arrow keys scroll by whole rows; kept so existing settings files still load
    float zoomSensitivity = 0.1f; // Unused since zoom moves in multiplicative steps; kept so existing settings files still load

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ControlSettings, arrowSensitivity, zoomSensitivity);
};

struct Settings
{
    DisplaySettings displaySettings;
    ControlSettings controlSettings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Settings, displaySettings, controlSettings);
};

inline std::optional<Settings> loadSettings(const std::string& jsonPath)
{
    try {
        if (auto json = loadSettingsFromJson(jsonPath)) {
            return json->get<Settings>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading settings: " << e.what() << std::endl;
    }
    return std::nullopt;
}
inline void saveSettings(const Settings& settings, const std::string& jsonPath)
{
    try {
        nlohmann::json json = settings;
        saveSettingsJson(json, jsonPath);
    } catch (const std::exception& e) {
        std::cerr << "Error saving settings: " << e.what() << std::endl;
    }
}