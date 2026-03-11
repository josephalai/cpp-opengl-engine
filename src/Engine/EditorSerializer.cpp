// src/Engine/EditorSerializer.cpp

#include "EditorSerializer.h"
#include "../ECS/Components/EditorPlacedComponent.h"
#include "../ECS/Components/TransformComponent.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

bool EditorSerializer::saveToJson(const std::string& outputPath,
                                  entt::registry&    registry) {
    // --- 1. Read existing file (preserve all other keys) ---
    nlohmann::json root;
    {
        std::ifstream in(outputPath);
        if (in.is_open()) {
            try {
                in >> root;
            } catch (const std::exception& e) {
                std::cerr << "[EditorSerializer] Failed to parse existing scene.json: "
                          << e.what() << "\n";
                // Start with an empty object so we can still write.
                root = nlohmann::json::object();
            }
        }
    }

    // --- 2. Build the editor_entities array ---
    nlohmann::json editorArr = nlohmann::json::array();

    auto view = registry.view<EditorPlacedComponent, TransformComponent>();
    for (auto entity : view) {
        const auto& epc = view.get<EditorPlacedComponent>(entity);
        const auto& tc  = view.get<TransformComponent>(entity);

        nlohmann::json entry;
        entry["alias"] = epc.prefabAlias;
        entry["x"]     = tc.position.x;
        entry["y"]     = tc.position.y;
        entry["z"]     = tc.position.z;
        entry["ry"]    = epc.rotationY;
        entry["scale"] = tc.scale;

        editorArr.push_back(entry);
    }

    // --- 3. Replace the editor_entities key and write back ---
    root["editor_entities"] = editorArr;

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "[EditorSerializer] Cannot write to: " << outputPath << "\n";
        return false;
    }
    out << root.dump(2) << "\n";
    std::cout << "[EditorSerializer] Saved " << editorArr.size()
              << " editor entities to " << outputPath << "\n";
    return true;
}
