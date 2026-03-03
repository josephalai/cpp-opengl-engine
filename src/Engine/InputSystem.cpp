// src/Engine/InputSystem.cpp

#include "InputSystem.h"
#include "../Entities/PlayerCamera.h"
#include "../Terrain/Terrain.h"
#include "../Toolbox/TerrainPicker.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Guis/Text/GUIText.h"

InputSystem::InputSystem(PlayerCamera* camera,
                         Terrain*       primaryTerrain,
                         TerrainPicker* picker,
                         GuiTexture*    sampleGui,
                         GUIText*       pNameText)
    : camera_(camera)
    , terrain_(primaryTerrain)
    , picker_(picker)
    , sampleGui_(sampleGui)
    , pNameText_(pNameText)
{}

void InputSystem::update(float /*deltaTime*/) {
    if (camera_)  camera_->move(terrain_);
    if (picker_)  picker_->update();

    // Simple per-frame GUI animations
    if (sampleGui_) sampleGui_->getPosition() += glm::vec2(0.001f, 0.001f);
    if (pNameText_) pNameText_->getPosition() += glm::vec2(0.1f);
}
