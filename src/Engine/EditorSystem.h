// src/Engine/EditorSystem.h
// ISystem that renders the Dear ImGui World Editor overlay and handles
// WYSIWYG object placement, entity selection, and map serialisation.
//
// This system owns the per-frame ImGui render commands (NewFrame / Render).
// It must be the *last* system that writes ImGui draw commands each frame
// so it runs at the end of the system pipeline.

#ifndef ENGINE_EDITOR_SYSTEM_H
#define ENGINE_EDITOR_SYSTEM_H

#include "ISystem.h"
#include "EditorState.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

class TerrainPicker;
class PhysicsSystem;
class MasterRenderer;

class EditorSystem : public ISystem {
public:
    EditorSystem(entt::registry& registry,
                 TerrainPicker*  picker,
                 PhysicsSystem*  physicsSystem,
                 MasterRenderer* renderer,
                 EditorState&    editorState,
                 const std::string& sceneJsonPath);

    void init()                  override {}
    void update(float deltaTime) override;
    void shutdown()              override {}

private:
    entt::registry& registry_;
    TerrainPicker*  picker_;
    PhysicsSystem*  physicsSystem_;
    MasterRenderer* renderer_;
    EditorState&    editorState_;
    std::string     sceneJsonPath_;

    // Cached list of prefab IDs (refreshed once on first update).
    std::vector<std::string> prefabIds_;
    int                      selectedPrefabIndex_ = -1;

    // Left-click placement edge detection.
    bool prevLeftClick_ = false;

    // --- Helper methods ---
    void renderEditorWindow();
    void handleGhostPreview();
    void handlePlacement();
    void renderEntityList();
    void renderTransformEditor();
    void handleEntityDeletion();
};

#endif // ENGINE_EDITOR_SYSTEM_H
