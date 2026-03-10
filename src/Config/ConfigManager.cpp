// src/Config/ConfigManager.cpp

#include "ConfigManager.h"

#include "../Util/FileSystem.h"

#include <nlohmann/json.hpp>
#include <iostream>

// -------------------------------------------------------------------------
// EnvironmentConfig helpers
// -------------------------------------------------------------------------

const EnvironmentPreset& EnvironmentConfig::current() const {
    auto it = presets.find(activePreset);
    if (it != presets.end()) return it->second;
    // Fallback: return first preset if available, else a static default.
    if (!presets.empty()) return presets.begin()->second;
    static const EnvironmentPreset kDefault;
    return kDefault;
}

// -------------------------------------------------------------------------
// Singleton
// -------------------------------------------------------------------------

ConfigManager& ConfigManager::get() {
    static ConfigManager instance;
    return instance;
}

// -------------------------------------------------------------------------
// loadAll — convenience: loads every config file from the resource root.
// -------------------------------------------------------------------------

void ConfigManager::loadAll(const std::string& resourceRoot) {
    const std::string base = resourceRoot + "/src/Resources/";
    loadWorldConfig(base + "world_config.json");
    loadClientSettings(base + "client_settings.json");
    loadEnvironmentPresets(base + "environment_presets.json");
    loaded_ = true;
}

// -------------------------------------------------------------------------
// JSON helper — read a vec3 from a JSON object (array [x,y,z] or {x,y,z}).
// -------------------------------------------------------------------------

static glm::vec3 readVec3(const nlohmann::json& j, const std::string& key,
                           const glm::vec3& fallback) {
    if (!j.contains(key)) return fallback;
    const auto& v = j[key];
    if (v.is_array() && v.size() >= 3) {
        return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
    }
    if (v.is_object()) {
        return {v.value("x", fallback.x),
                v.value("y", fallback.y),
                v.value("z", fallback.z)};
    }
    return fallback;
}

// -------------------------------------------------------------------------
// world_config.json
// -------------------------------------------------------------------------

void ConfigManager::loadWorldConfig(const std::string& path) {
    auto bytes = FileSystem::readAllBytes(path);
    if (bytes.empty()) {
        std::cerr << "[ConfigManager] Could not open " << path
                  << " — using defaults.\n";
        return;
    }

    nlohmann::json root;
    try { root = nlohmann::json::parse(bytes.begin(), bytes.end()); }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[ConfigManager] JSON parse error in " << path
                  << ": " << e.what() << "\n";
        return;
    }

    // Physics
    if (root.contains("physics")) {
        const auto& p = root["physics"];
        physics.gravity          = readVec3(p, "gravity", physics.gravity);
        physics.jumpPower        = p.value("jump_power", physics.jumpPower);
        physics.defaultRunSpeed  = p.value("default_run_speed", physics.defaultRunSpeed);
        physics.defaultTurnSpeed = p.value("default_turn_speed", physics.defaultTurnSpeed);
        physics.npcTurnSpeed     = p.value("npc_turn_speed", physics.npcTurnSpeed);
        physics.terrainSize      = p.value("terrain_size", physics.terrainSize);

        physics.defaultCapsuleRadius = p.value("default_capsule_radius", physics.defaultCapsuleRadius);
        physics.defaultCapsuleHeight = p.value("default_capsule_height", physics.defaultCapsuleHeight);
        physics.defaultStepHeight    = p.value("default_step_height", physics.defaultStepHeight);
        physics.defaultMass          = p.value("default_mass", physics.defaultMass);

        physics.sprintMultiplier     = p.value("sprint_multiplier", physics.sprintMultiplier);
    }

    // Default spawn position (top-level in world_config.json)
    physics.defaultSpawnPosition = readVec3(root, "default_spawn_position",
                                            physics.defaultSpawnPosition);

    // Server
    if (root.contains("server")) {
        const auto& s = root["server"];
        server.port         = s.value("port", server.port);
        server.maxClients   = s.value("max_clients", server.maxClients);
        server.channelCount = s.value("channel_count", server.channelCount);
        server.tickInterval = s.value("tick_interval", server.tickInterval);
    }

    std::cout << "[ConfigManager] Loaded world config from " << path << "\n";
}

// -------------------------------------------------------------------------
// client_settings.json
// -------------------------------------------------------------------------

void ConfigManager::loadClientSettings(const std::string& path) {
    auto bytes = FileSystem::readAllBytes(path);
    if (bytes.empty()) {
        std::cerr << "[ConfigManager] Could not open " << path
                  << " — using defaults.\n";
        return;
    }

    nlohmann::json root;
    try { root = nlohmann::json::parse(bytes.begin(), bytes.end()); }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[ConfigManager] JSON parse error in " << path
                  << ": " << e.what() << "\n";
        return;
    }

    client.windowWidth  = root.value("window_width",  client.windowWidth);
    client.windowHeight = root.value("window_height", client.windowHeight);
    client.fov          = root.value("fov",           client.fov);
    client.nearPlane    = root.value("near_plane",    client.nearPlane);
    client.farPlane     = root.value("far_plane",     client.farPlane);
    client.windowTitle  = root.value("window_title",  client.windowTitle);

    // Camera settings
    if (root.contains("camera")) {
        const auto& cam = root["camera"];
        client.camera.mouseSensitivity = cam.value("mouse_sensitivity", client.camera.mouseSensitivity);
        client.camera.zoomSensitivity  = cam.value("zoom_sensitivity",  client.camera.zoomSensitivity);
        client.camera.minZoomDistance   = cam.value("min_zoom",          client.camera.minZoomDistance);
        client.camera.maxZoomDistance   = cam.value("max_zoom",          client.camera.maxZoomDistance);
        client.camera.pitchOffset      = cam.value("pitch_offset",      client.camera.pitchOffset);
    }

    std::cout << "[ConfigManager] Loaded client settings from " << path << "\n";
}

// -------------------------------------------------------------------------
// environment_presets.json
// -------------------------------------------------------------------------

void ConfigManager::loadEnvironmentPresets(const std::string& path) {
    auto bytes = FileSystem::readAllBytes(path);
    if (bytes.empty()) {
        std::cerr << "[ConfigManager] Could not open " << path
                  << " — using defaults.\n";
        return;
    }

    nlohmann::json root;
    try { root = nlohmann::json::parse(bytes.begin(), bytes.end()); }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[ConfigManager] JSON parse error in " << path
                  << ": " << e.what() << "\n";
        return;
    }

    environment.activePreset = root.value("active_preset", environment.activePreset);

    if (root.contains("presets") && root["presets"].is_object()) {
        for (auto& [name, preset] : root["presets"].items()) {
            EnvironmentPreset ep;
            ep.name         = name;
            ep.skyColor     = readVec3(preset, "sky_color",      ep.skyColor);
            ep.fogDensity   = preset.value("fog_density",        ep.fogDensity);
            ep.fogStart     = preset.value("fog_start",          ep.fogStart);
            ep.fogEnd       = preset.value("fog_end",            ep.fogEnd);
            ep.fogColor     = readVec3(preset, "fog_color",      ep.fogColor);
            ep.ambientLight = readVec3(preset, "ambient_light",  ep.ambientLight);
            ep.sunDirection = readVec3(preset, "sun_direction",   ep.sunDirection);
            ep.sunColor     = readVec3(preset, "sun_color",      ep.sunColor);
            environment.presets[name] = ep;
        }
    }

    std::cout << "[ConfigManager] Loaded " << environment.presets.size()
              << " environment preset(s) from " << path << "\n";
}
