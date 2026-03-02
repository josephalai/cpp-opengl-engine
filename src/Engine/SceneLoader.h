//
// SceneLoader: reads a plain-text scene.cfg file and populates the engine's
// scene vectors (entities, lights, terrains, GUIs, player, camera).
// Edit src/Resources/Tutorial/scene.cfg to change what the engine loads at
// runtime without recompiling.
//

#ifndef ENGINE_SCENELOADER_H
#define ENGINE_SCENELOADER_H

#include <string>
#include <vector>
#include <map>

#include "../RenderEngine/Loader.h"
#include "../Entities/Entity.h"
#include "../Entities/AssimpEntity.h"
#include "../Entities/Player.h"
#include "../Entities/PlayerCamera.h"
#include "../Entities/Light.h"
#include "../Terrain/Terrain.h"
#include "../Textures/TerrainTexturePack.h"
#include "../Guis/Texture/GuiTexture.h"
#include "../Models/TexturedModel.h"
#include "../RenderEngine/ObjLoader.h"
#include "../BoundingBox/BoundingBoxIndex.h"

/// Reads src/Resources/Tutorial/scene.cfg and loads the described 3-D scene
/// content into the vectors/pointers passed by reference.  Returns true on
/// success; returns false (and prints a warning) if the file cannot be opened,
/// in which case the caller should fall back to hard-coded defaults.
class SceneLoader {
public:
    static bool load(
        const std::string&          configPath,
        Loader*                     loader,
        std::vector<Entity*>&       entities,
        std::vector<AssimpEntity*>& scenes,
        std::vector<Light*>&        lights,
        std::vector<Terrain*>&      allTerrains,
        std::vector<GuiTexture*>&   guis,
        Terrain*&                   primaryTerrain,
        Player*&                    player,
        PlayerCamera*&              playerCamera
    );

private:
    // -----------------------------------------------------------------------
    // Parsed record types (internal to one load() call)
    // -----------------------------------------------------------------------

    struct ModelDef {
        std::string alias;
        std::string objFile;
        std::string textureFile;
        bool        transparent  = false;
        bool        fakeLighting = false;
        int         atlasRows    = 1;
        float       shininess    = 1.0f;
        float       reflectivity = 0.5f;
    };

    struct AssimpDef {
        std::string path;
        bool        randomPos  = false;
        float       x = 0, y = 0, z = 0;
        float       yOffset    = 0.0f;
        float       scaleMin   = 1.0f;
        float       scaleMax   = 1.0f;
    };

    struct LightDef {
        bool  directional = false;
        float x = 0, y = 0, z = 0;
        bool  snapY       = false;   ///< true → snap y to terrain + yOffset
        float yOffset     = 0.0f;
        float r = 0, g = 0, b = 0;
        float attenDist   = 65.0f;
    };

    struct TerrainDef {
        std::string heightmapFile = "heightMap";
        std::string blendmapFile  = "blendMap";
    };

    struct TerrainTileDef {
        int gridX = 0;
        int gridZ = -1;
        bool primary = false;  ///< first tile becomes primaryTerrain
    };

    struct EntityDef {
        std::string alias;
        float x = 0, y = 0, z = 0;
        bool  snapY  = false;    ///< true → snap y to terrain height
        float rx = 0, ry = 0, rz = 0;
        float scale  = 1.0f;
        bool  useAtlas = false;
    };

    struct RandomDef {
        std::string alias;
        int   count    = 0;
        float scaleMin = 0.75f;
        float scaleMax = 1.50f;
        bool  useAtlas = false;
    };

    struct PlayerDef {
        std::string alias;
        float x = 0, y = 0, z = 0;
        float rx = 0, ry = 0, rz = 0;
        float scale = 1.0f;
    };

    struct GuiDef {
        std::string textureFile;
        float x = 0, y = 0;
        float w = 0, h = 0;
    };

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    static std::vector<std::string> tokenize(const std::string& line);
    static float randomFloat();
    static float randomScale(float mn, float mx);
    static glm::vec3 randomPosition(Terrain* terrain, float yOffset = 0.0f);
    static glm::vec3 randomRotation();

    // Sentinel: when y == kSnapY the entity's y is resolved from terrain
    static constexpr float kSnapY = -99999.0f;
};

#endif // ENGINE_SCENELOADER_H
