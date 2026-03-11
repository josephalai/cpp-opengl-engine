// src/Engine/InputSystem.h
// Subsystem responsible for camera movement and input-driven per-frame updates.
// Wraps the GLFW-callback-based InputMaster and drives PlayerCamera movement.
// When EditorState::isEditorMode is true, the orbit camera is suppressed and
// a free-fly god-mode camera takes over instead.

#ifndef ENGINE_INPUTSYSTEM_H
#define ENGINE_INPUTSYSTEM_H

#include "ISystem.h"
#include "EditorState.h"

class PlayerCamera;
class Terrain;
class TerrainPicker;
class GuiTexture;
class GUIText;

class InputSystem : public ISystem {
public:
    InputSystem(PlayerCamera* camera,
                Terrain*      primaryTerrain,
                TerrainPicker* picker,
                GuiTexture*   sampleGui,
                GUIText*      pNameText,
                EditorState*  editorState = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    PlayerCamera*  camera_;
    Terrain*       terrain_;
    TerrainPicker* picker_;
    GuiTexture*    sampleGui_;
    GUIText*       pNameText_;
    EditorState*   editorState_;

    /// Drive a free-fly god-mode camera (WASD + right-mouse look).
    void updateGodCamera(float deltaTime);
};

#endif // ENGINE_INPUTSYSTEM_H
