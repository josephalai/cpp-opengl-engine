//
// Created by Joseph Alai on 7/3/21.
//

#ifndef ENGINE_MASTERRENDERER_H
#define ENGINE_MASTERRENDERER_H

#include <map>
#include <iostream>
#include "../Shaders/StaticShader.h"
#include "../Entities/CameraInput.h"
#include "EntityRenderer.h"
#include "TerrainRenderer.h"
#include "AssimpEntityRenderer.h"
#include "../Shaders/AssimpStaticShader.h"
#include "AssimpEntityLoader.h"
#include "../Entities/PlayerCamera.h"
#include "../Skybox/SkyboxRenderer.h"
#include "../BoundingBox/BoundingBoxShader.h"
#include "../BoundingBox/BoundingBoxRenderer.h"
#include "../Toolbox/Color.h"
#include "../Atmosphere/FogSettings.h"
#include "../Scene/SceneGraph.h"
#include "../Shadows/ShadowMap.h"
#include "../Shadows/ShadowShader.h"
#include "../Water/WaterRenderer.h"
#include "../Water/WaterTile.h"
#include "../Shaders/PBRShader.h"
#include "../Shaders/PBRMaterial.h"
#include "InstancedRenderer.h"
#include "InstancedModel.h"
#include "../Shaders/InstancedShader.h"

static const float FOVY = 45.0f;
static const float NEAR_PLANE = 0.1f;
static const float FAR_PLANE = 1000;

class MasterRenderer {
private:
    PlayerCamera *camera;

    StaticShader *shader;
    AssimpStaticShader *sceneShader;
    TerrainShader *terrainShader;
    BoundingBoxShader *bShader;

    EntityRenderer *renderer;
    SkyboxRenderer *skyboxRenderer;
    TerrainRenderer *terrainRenderer;
    AssimpEntityRenderer *sceneRenderer;
    BoundingBoxRenderer *bRenderer;

    std::map<RawBoundingBox *, std::vector<Entity *>> *boxes;
    std::map<TexturedModel *, std::vector<Entity *>> *entities;
    std::map<AssimpMesh *, std::vector<AssimpModelComponent>> *scenes;
    std::vector<Terrain *> *terrains;

    glm::mat4 projectionMatrix;

    // --- New subsystems ---
    FogSettings fogSettings;

    // Shadow mapping (optional — nullptr means disabled)
    ShadowMap    *shadowMap    = nullptr;
    ShadowShader *shadowShader = nullptr;

    // Water rendering (optional)
    WaterRenderer *waterRenderer = nullptr;
    std::vector<WaterTile> waterTiles;

    // PBR shader (optional)
    PBRShader *pbrShader = nullptr;

    // Instanced rendering
    InstancedShader   *instancedShader   = nullptr;
    InstancedRenderer *instancedRenderer = nullptr;

public:
    explicit MasterRenderer(PlayerCamera *cameraInput, Loader *loader);

    void cleanUp();

    /**
     * @brief prepares and clears buffer and screen for each iteration of loop
     */
    void prepare();

    static Color skyColor;

    void render(const std::vector<Light *> &sun);

    void processTerrain(Terrain *terrain);

    static glm::mat4 createProjectionMatrix();

    glm::mat4 getProjectionMatrix();

    void processEntity(Entity *entity);

    void processAssimpEntity(const AssimpModelComponent& comp);

    void processBoundingBox(Entity *entity);

    void renderScene(std::vector<Entity *> entities, std::vector<AssimpModelComponent> aComps,
                     std::vector<Terrain *> terrains, std::vector<Light *> lights);

    void renderBoundingBoxes(std::vector<Entity *> entities);

    void prepareBoundingBoxRender();

    void render();

    // --- Fog / Atmosphere ---
    FogSettings& getFogSettings() { return fogSettings; }
    void setFogSettings(const FogSettings& fs) { fogSettings = fs; }

    // --- Shadow mapping ---
    void enableShadowMapping(int mapSize = ShadowMap::kDefaultSize);
    ShadowMap* getShadowMap() { return shadowMap; }

    /// Run the shadow-map depth pass before the main render.
    void renderShadowPass(const std::vector<Entity*>& allEntities,
                          const std::vector<Light*>&  lights);

    // --- Water ---
    void addWaterTile(const WaterTile& tile) { waterTiles.push_back(tile); }
    void setWaterRenderer(WaterRenderer* wr) { waterRenderer = wr; }
    WaterRenderer* getWaterRenderer() { return waterRenderer; }

    /// Render water tiles.  Call after renderScene().
    void renderWater(Camera* cam, Light* sun);

    // --- Scene Graph ---
    void renderSceneGraph(SceneGraph& graph, std::vector<AssimpModelComponent> aComps,
                          std::vector<Terrain*> terrains, std::vector<Light*> lights);

    // --- PBR ---
    PBRShader* getPBRShader() { return pbrShader; }

    // --- Instanced Rendering ---
    void processInstancedEntity(InstancedModel* model, const std::vector<glm::mat4>& transforms);
    void renderInstanced(const std::vector<Light*>& lights);
};

#endif //ENGINE_MASTERRENDERER_H
