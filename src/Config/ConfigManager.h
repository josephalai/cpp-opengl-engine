// src/Config/ConfigManager.h
//
// Singleton configuration manager for the data-driven engine architecture.
// Loads typed configuration structs from JSON files on boot so that every
// subsystem (Physics, Rendering, NPC AI, etc.) can pull its tunables from
// data instead of hardcoding magic numbers in C++.
//
// Usage:
//   ConfigManager::get().loadAll(resourceRoot);   // once at boot
//   float g = ConfigManager::get().physics.gravity.y;

#ifndef ENGINE_CONFIG_MANAGER_H
#define ENGINE_CONFIG_MANAGER_H

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

// -------------------------------------------------------------------------
// Physics / World configuration  (world_config.json)
// -------------------------------------------------------------------------
struct PhysicsConfig {
    glm::vec3 gravity         = {0.0f, -50.0f, 0.0f};
    float     jumpPower       = 30.0f;
    float     defaultRunSpeed = 20.0f;
    float     defaultTurnSpeed= 160.0f;
    float     npcTurnSpeed    = 80.0f;
    float     terrainSize     = 800.0f;

    /// Per-entity character-controller defaults.
    float     defaultCapsuleRadius = 0.5f;
    float     defaultCapsuleHeight = 1.8f;
    float     defaultStepHeight    = 0.35f;
    float     defaultMass          = 70.0f;

    /// Default spawn position for new players.
    glm::vec3 defaultSpawnPosition = {100.0f, 3.0f, -80.0f};

    /// Sprint speed multiplier applied when the sprint key is held.
    float     sprintMultiplier = 4.5f;
};

// -------------------------------------------------------------------------
// Client rendering settings  (client_settings.json)
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// Camera configuration  (camera block inside client_settings.json)
// -------------------------------------------------------------------------
struct CameraConfig {
    float mouseSensitivity = 0.1f;
    float zoomSensitivity  = 0.03f;
    float minZoomDistance   = 5.0f;
    float maxZoomDistance   = 50.0f;
    float pitchOffset      = 4.0f;
};

struct ClientConfig {
    int         windowWidth   = 800;
    int         windowHeight  = 600;
    float       fov           = 45.0f;
    float       nearPlane     = 0.1f;
    float       farPlane      = 1000.0f;
    std::string windowTitle   = "star wars scaperune";
    CameraConfig camera;
};

// -------------------------------------------------------------------------
// Environment preset  (one entry in environment_presets.json)
// -------------------------------------------------------------------------
struct EnvironmentPreset {
    std::string name;
    glm::vec3   skyColor        = {0.53f, 0.81f, 0.92f}; // Skyblue default
    float       fogDensity      = 0.007f;
    float       fogStart        = 10.0f;
    float       fogEnd          = 800.0f;
    glm::vec3   fogColor        = {0.5f, 0.6f, 0.7f};
    glm::vec3   ambientLight    = {0.4f, 0.4f, 0.4f};
    glm::vec3   sunDirection    = {0.0f, -1.0f, -0.3f};
    glm::vec3   sunColor        = {1.0f, 0.9f, 0.7f};
};

// -------------------------------------------------------------------------
// Environment configuration  (environment_presets.json)
// -------------------------------------------------------------------------
struct EnvironmentConfig {
    std::string activePreset = "default";
    std::unordered_map<std::string, EnvironmentPreset> presets;

    /// Return the currently active preset (falls back to a default-constructed
    /// EnvironmentPreset if the named preset is missing).
    const EnvironmentPreset& current() const;
};

// -------------------------------------------------------------------------
// Server configuration  (subset of world_config.json)
// -------------------------------------------------------------------------
struct ServerConfig {
    int   port         = 7777;
    int   maxClients   = 32;
    int   channelCount = 2;
    float tickInterval = 0.1f;  // seconds (10 Hz)
};

// =========================================================================
// ConfigManager — singleton that owns all config structs
// =========================================================================
class ConfigManager {
public:
    /// Singleton accessor.
    static ConfigManager& get();

    /// Load every config file from the given resource root.
    /// Typically called once at boot with the RESOURCE_ROOT path.
    void loadAll(const std::string& resourceRoot);

    /// Load individual config files (called by loadAll, also usable standalone).
    void loadWorldConfig(const std::string& path);
    void loadClientSettings(const std::string& path);
    void loadEnvironmentPresets(const std::string& path);

    // --- Public config structs (read freely after loadAll) ---
    PhysicsConfig     physics;
    ClientConfig      client;
    EnvironmentConfig environment;
    ServerConfig      server;

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    bool loaded_ = false;
};

#endif // ENGINE_CONFIG_MANAGER_H
