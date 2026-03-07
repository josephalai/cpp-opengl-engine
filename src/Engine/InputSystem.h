// src/Engine/InputSystem.h
// Subsystem responsible for camera movement, input-driven player movement,
// and input-driven per-frame updates.
//
// Phase 2 Step 3: InputSystem now owns the per-frame player movement logic
// that was previously in InputComponent::update().  It reads the public data
// fields of the attached InputComponent and applies rotation and physics
// walk-direction (or legacy terrain movement) each frame.

#ifndef ENGINE_INPUTSYSTEM_H
#define ENGINE_INPUTSYSTEM_H

#include "ISystem.h"

class PlayerCamera;
class Terrain;
class TerrainPicker;
class GuiTexture;
class GUIText;
class Player;
class PhysicsSystem;

class InputSystem : public ISystem {
public:
    InputSystem(PlayerCamera*  camera,
                Terrain*       primaryTerrain,
                TerrainPicker* picker,
                GuiTexture*    sampleGui,
                GUIText*       pNameText,
                Player*        player      = nullptr,
                PhysicsSystem* physics     = nullptr);

    void init()     override {}
    void update(float deltaTime) override;
    void shutdown() override {}

private:
    PlayerCamera*  camera_;
    Terrain*       terrain_;
    TerrainPicker* picker_;
    GuiTexture*    sampleGui_;
    GUIText*       pNameText_;
    Player*        player_;
    PhysicsSystem* physicsSystem_;
};

#endif // ENGINE_INPUTSYSTEM_H
