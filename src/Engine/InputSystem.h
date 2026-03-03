// src/Engine/InputSystem.h
// Subsystem responsible for camera movement and input-driven per-frame updates.
// Wraps the GLFW-callback-based InputMaster and drives PlayerCamera movement.

#ifndef ENGINE_INPUTSYSTEM_H
#define ENGINE_INPUTSYSTEM_H

#include "ISystem.h"

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
                GUIText*      pNameText);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    PlayerCamera*  camera_;
    Terrain*       terrain_;
    TerrainPicker* picker_;
    GuiTexture*    sampleGui_;
    GUIText*       pNameText_;
};

#endif // ENGINE_INPUTSYSTEM_H
