//
// SceneLoader.cpp — parses scene.cfg and loads the described scene objects.
//
// Config format (see src/Resources/scene.cfg for a fully-commented example):
//
//   # comment
//   terrain <heightmapFile> <blendmapFile>
//   terrain_tile <gridX> <gridZ> [primary]
//   light directional <x> <y> <z> <r> <g> <b>
//   light point <x> [terrain[+offset]|<y>] <z> <r> <g> <b> [dist=<N>]
//   model <alias> <objFile> <textureFile> [atlas=N] [transparent] [fakeLighting]
//         [shininess=F] [reflectivity=F]
//   entity <alias> <x> [terrain[+offset]|<y>] <z> [rx ry rz] [scale=F]
//   random <alias> <count> [scaleMin=F] [scaleMax=F] [atlas]
//   player <alias> <x> <y> <z> [rx ry rz] [scale=F]
//   assimp <path> [random[+offset]|<x> <y> <z>] [scaleMin=F scaleMax=F]
//   gui <textureFile> <x> <y> <w> <h>
//   text <font> <size> <x> <y> [maxWidth=F] [color=R,G,B] [centered] "message"
//   animated_character <path> <x> <y|terrain[+N]> <z> [scale=F] [rot=RX,RY,RZ]
//

#include "SceneLoader.h"
#include "../Util/FileSystem.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Util/LightUtil.h"
#include "../Textures/TerrainTexture.h"
#include "../Interaction/InteractiveModel.h"
#include "../Guis/Text/FontMeshCreator/TextMeshData.h"
#include "../Toolbox/Color.h"
#include "../Animation/AnimationLoader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

std::vector<std::string> SceneLoader::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    const size_t n = line.size();
    size_t i = 0;
    while (i < n) {
        // skip whitespace
        if (std::isspace(static_cast<unsigned char>(line[i]))) { ++i; continue; }
        // comment — stop here
        if (line[i] == '#') break;
        // quoted string — collect everything between the quotes as one token
        if (line[i] == '"') {
            ++i;
            std::string tok;
            while (i < n && line[i] != '"') tok += line[i++];
            if (i < n) ++i;  // consume closing "
            tokens.push_back(tok);
            continue;
        }
        // plain token — stop at whitespace or '#'
        std::string tok;
        while (i < n && !std::isspace(static_cast<unsigned char>(line[i])) && line[i] != '#')
            tok += line[i++];
        if (!tok.empty()) tokens.push_back(tok);
    }
    return tokens;
}

float SceneLoader::randomFloat() {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float SceneLoader::randomScale(float mn, float mx) {
    float multiplier = (mx > 1.0f) ? std::ceil(mx) : 1.0f;
    float r = randomFloat() * multiplier;
    if (r < mn) r = mn;
    if (r > mx) r = mx;
    return r;
}

glm::vec3 SceneLoader::randomPosition(Terrain* terrain, float yOffset) {
    glm::vec3 p;
    p.x = std::floor(randomFloat() * 1500.f - 800.f);
    p.z = std::floor(randomFloat() * -800.f);
    p.y = terrain->getHeightOfTerrain(p.x, p.z) + yOffset;
    return p;
}

glm::vec3 SceneLoader::randomRotation() {
    float ry = (randomFloat() * 100.f - 50.f) * 180.0f;
    return glm::vec3(0.0f, ry, 0.0f);
}

// ---------------------------------------------------------------------------
// Parse a "y token": either a plain float, or "terrain" / "terrain+N"
// Returns kSnapY sentinel + stores offset when terrain-relative.
// ---------------------------------------------------------------------------
static float parseY(const std::string& tok, float& yOffset) {
    yOffset = 0.0f;
    if (tok.rfind("terrain", 0) == 0) {
        // terrain  OR  terrain+3.5  OR  terrain-1.0
        std::string rest = tok.substr(7);
        if (!rest.empty()) {
            yOffset = std::stof(rest);  // handles "+N" and "-N"
        }
        return SceneLoader::kSnapY;
    }
    return std::stof(tok);
}

// Parse an "option=value" token.  Returns the value string or "".
static std::string optVal(const std::string& tok, const std::string& key) {
    if (tok.rfind(key + "=", 0) == 0)
        return tok.substr(key.size() + 1);
    return {};
}

// ---------------------------------------------------------------------------
// SceneLoader::load
// ---------------------------------------------------------------------------

bool SceneLoader::load(
    const std::string&          configPath,
    Loader*                     loader,
    std::vector<Entity*>&       entities,
    std::vector<AssimpEntity*>& scenes,
    std::vector<Light*>&        lights,
    std::vector<Terrain*>&      allTerrains,
    std::vector<GuiTexture*>&   guis,
    std::vector<GUIText*>&      texts,
    std::vector<WaterTile>&     waterTiles,
    Terrain*&                   primaryTerrain,
    Player*&                    player,
    PlayerCamera*&              playerCamera,
    std::vector<AnimatedEntity*>& animatedEntities)
{
    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[SceneLoader] Cannot open: " << configPath << std::endl;
        return false;
    }
    std::cout << "[SceneLoader] Loading scene from: " << configPath << std::endl;

    // -----------------------------------------------------------------------
    // Pass 1: parse the whole file into definition structs
    // -----------------------------------------------------------------------
    TerrainDef                 terrainDef;
    std::vector<TerrainTileDef> tileDefs;
    std::vector<ModelDef>      modelDefs;
    std::vector<AssimpDef>     assimpDefs;
    std::vector<LightDef>      lightDefs;
    std::vector<EntityDef>     entityDefs;
    std::vector<RandomDef>     randomDefs;
    PlayerDef                  playerDef;
    bool                       hasPlayer = false;
    std::vector<GuiDef>        guiDefs;
    std::vector<TextDef>       textDefs;
    std::vector<WaterDef>      waterDefs;
    std::vector<AnimCharDef>   animCharDefs;

    std::string line;
    int lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;
        // strip inline comments and tokenize
        auto tokens = tokenize(line);
        if (tokens.empty()) continue;

        const std::string& cmd = tokens[0];

        // ----------------------------------------------------------------
        // terrain <heightmapFile> <blendmapFile>
        if (cmd == "terrain") {
            if (tokens.size() >= 3) {
                terrainDef.heightmapFile = tokens[1];
                terrainDef.blendmapFile  = tokens[2];
            }
        }
        // ----------------------------------------------------------------
        // terrain_tile <gridX> <gridZ> [primary]
        else if (cmd == "terrain_tile") {
            if (tokens.size() >= 3) {
                TerrainTileDef t;
                t.gridX   = std::stoi(tokens[1]);
                t.gridZ   = std::stoi(tokens[2]);
                t.primary = (tokens.size() >= 4 && tokens[3] == "primary");
                tileDefs.push_back(t);
            }
        }
        // ----------------------------------------------------------------
        // light directional <x> <y> <z> <r> <g> <b>
        // light point       <x> <y|terrain[+N]> <z> <r> <g> <b> [dist=N]
        else if (cmd == "light") {
            if (tokens.size() >= 8) {
                LightDef ld;
                ld.directional = (tokens[1] == "directional");
                ld.x = std::stof(tokens[2]);
                float yo = 0.0f;
                ld.y = parseY(tokens[3], yo);
                if (ld.y == kSnapY) { ld.snapY = true; ld.yOffset = yo; }
                ld.z = std::stof(tokens[4]);
                ld.r = std::stof(tokens[5]);
                ld.g = std::stof(tokens[6]);
                ld.b = std::stof(tokens[7]);
                for (size_t i = 8; i < tokens.size(); ++i) {
                    auto v = optVal(tokens[i], "dist");
                    if (!v.empty()) ld.attenDist = std::stof(v);
                }
                lightDefs.push_back(ld);
            }
        }
        // ----------------------------------------------------------------
        // model <alias> <objFile> <textureFile> [options…]
        else if (cmd == "model") {
            if (tokens.size() >= 4) {
                ModelDef md;
                md.alias       = tokens[1];
                md.objFile     = tokens[2];
                md.textureFile = tokens[3];
                for (size_t i = 4; i < tokens.size(); ++i) {
                    if (tokens[i] == "transparent")   { md.transparent  = true; continue; }
                    if (tokens[i] == "fakeLighting")  { md.fakeLighting = true; continue; }
                    auto v = optVal(tokens[i], "atlas");
                    if (!v.empty()) { md.atlasRows    = std::stoi(v); continue; }
                    v = optVal(tokens[i], "shininess");
                    if (!v.empty()) { md.shininess    = std::stof(v); continue; }
                    v = optVal(tokens[i], "reflectivity");
                    if (!v.empty()) { md.reflectivity = std::stof(v); continue; }
                }
                modelDefs.push_back(md);
            }
        }
        // ----------------------------------------------------------------
        // assimp <path> [random[+offset]|<x> <y> <z>] [scaleMin=F scaleMax=F]
        else if (cmd == "assimp") {
            if (tokens.size() >= 2) {
                AssimpDef ad;
                ad.path = tokens[1];
                if (tokens.size() >= 3 && tokens[2].rfind("random", 0) == 0) {
                    ad.randomPos = true;
                    std::string rest = tokens[2].substr(6);  // after "random"
                    if (!rest.empty()) ad.yOffset = std::stof(rest);
                    for (size_t i = 3; i < tokens.size(); ++i) {
                        auto v = optVal(tokens[i], "scaleMin");
                        if (!v.empty()) { ad.scaleMin = std::stof(v); continue; }
                        v = optVal(tokens[i], "scaleMax");
                        if (!v.empty()) { ad.scaleMax = std::stof(v); continue; }
                        // bare positional scaleMin scaleMax pair
                        if (i + 1 < tokens.size() && v.empty()) {
                            try {
                                ad.scaleMin = std::stof(tokens[i]);
                                ad.scaleMax = std::stof(tokens[i + 1]);
                                ++i;
                            } catch (...) {}
                        }
                    }
                } else if (tokens.size() >= 5) {
                    ad.x = std::stof(tokens[2]);
                    ad.y = std::stof(tokens[3]);
                    ad.z = std::stof(tokens[4]);
                }
                assimpDefs.push_back(ad);
            }
        }
        // ----------------------------------------------------------------
        // entity <alias> <x> <y|terrain[+N]> <z> [rx ry rz] [scale=F]
        else if (cmd == "entity") {
            if (tokens.size() >= 5) {
                EntityDef ed;
                ed.alias = tokens[1];
                ed.x     = std::stof(tokens[2]);
                float yo = 0.0f;
                ed.y     = parseY(tokens[3], yo);
                if (ed.y == kSnapY) { ed.snapY = true; }
                ed.z     = std::stof(tokens[4]);
                size_t i = 5;
                // optional rx ry rz (three bare floats)
                if (i + 2 < tokens.size()) {
                    try {
                        ed.rx = std::stof(tokens[i]);
                        ed.ry = std::stof(tokens[i + 1]);
                        ed.rz = std::stof(tokens[i + 2]);
                        i += 3;
                    } catch (...) {}
                }
                for (; i < tokens.size(); ++i) {
                    auto v = optVal(tokens[i], "scale");
                    if (!v.empty()) ed.scale = std::stof(v);
                }
                entityDefs.push_back(ed);
            }
        }
        // ----------------------------------------------------------------
        // random <alias> <count> [scaleMin=F] [scaleMax=F] [atlas]
        else if (cmd == "random") {
            if (tokens.size() >= 3) {
                RandomDef rd;
                rd.alias = tokens[1];
                rd.count = std::stoi(tokens[2]);
                // bare scaleMin scaleMax pair
                size_t i = 3;
                if (i + 1 < tokens.size()) {
                    try {
                        rd.scaleMin = std::stof(tokens[i]);
                        rd.scaleMax = std::stof(tokens[i + 1]);
                        i += 2;
                    } catch (...) {}
                }
                for (; i < tokens.size(); ++i) {
                    auto v = optVal(tokens[i], "scaleMin");
                    if (!v.empty()) { rd.scaleMin = std::stof(v); continue; }
                    v = optVal(tokens[i], "scaleMax");
                    if (!v.empty()) { rd.scaleMax = std::stof(v); continue; }
                    if (tokens[i] == "atlas") rd.useAtlas = true;
                }
                randomDefs.push_back(rd);
            }
        }
        // ----------------------------------------------------------------
        // player <alias> <x> <y> <z> [rx ry rz] [scale=F]
        else if (cmd == "player") {
            if (tokens.size() >= 5) {
                playerDef.alias = tokens[1];
                playerDef.x     = std::stof(tokens[2]);
                playerDef.y     = std::stof(tokens[3]);
                playerDef.z     = std::stof(tokens[4]);
                size_t i = 5;
                if (i + 2 < tokens.size()) {
                    try {
                        playerDef.rx = std::stof(tokens[i]);
                        playerDef.ry = std::stof(tokens[i + 1]);
                        playerDef.rz = std::stof(tokens[i + 2]);
                        i += 3;
                    } catch (...) {}
                }
                for (; i < tokens.size(); ++i) {
                    auto v = optVal(tokens[i], "scale");
                    if (!v.empty()) playerDef.scale = std::stof(v);
                }
                hasPlayer = true;
            }
        }
        // ----------------------------------------------------------------
        // gui <textureFile> <x> <y> <w> <h>
        else if (cmd == "gui") {
            if (tokens.size() >= 6) {
                GuiDef gd;
                gd.textureFile = tokens[1];
                gd.x = std::stof(tokens[2]);
                gd.y = std::stof(tokens[3]);
                gd.w = std::stof(tokens[4]);
                gd.h = std::stof(tokens[5]);
                guiDefs.push_back(gd);
            }
        }
        // ----------------------------------------------------------------
        // text <font> <size> <x> <y> [maxWidth=F] [color=R,G,B] [centered] "message"
        //
        // font    — font name (maps to Resources/Tutorial/Fonts/<name>.ttf)
        // size    — FreeType pixel size (e.g. 24, 36, 48)
        // x, y    — NDC position (-1..+1), top-left of text block
        // maxWidth— optional; max line width as fraction of screen width (0..1)
        // color   — optional; float RGB components 0..1 (e.g. color=1.0,0.84,0.0)
        // centered— optional keyword; centers the text within maxWidth
        // "message"— quoted text string (required, must be last)
        else if (cmd == "text") {
            // minimum: text <font> <size> <x> <y> "message" = 6 tokens
            if (tokens.size() >= 6) {
                TextDef td;
                td.fontName = tokens[1];
                try { td.fontSize = std::stoi(tokens[2]); } catch (...) {}
                try { td.x = std::stof(tokens[3]); } catch (...) {}
                try { td.y = std::stof(tokens[4]); } catch (...) {}
                // message is always the last token (quoted string)
                td.message = tokens.back();
                // parse optional tokens between position and message
                for (size_t i = 5; i + 1 < tokens.size(); ++i) {
                    if (tokens[i] == "centered") { td.centered = true; continue; }
                    auto v = optVal(tokens[i], "maxWidth");
                    if (!v.empty()) { td.maxWidth = std::stof(v); continue; }
                    v = optVal(tokens[i], "color");
                    if (!v.empty()) {
                        // parse "R,G,B"
                        std::istringstream cs(v);
                        std::string comp;
                        float* dst[3] = {&td.r, &td.g, &td.b};
                        int ci = 0;
                        while (std::getline(cs, comp, ',') && ci < 3) {
                            try { *dst[ci++] = std::stof(comp); } catch (...) {}
                        }
                    }
                }
                if (!td.message.empty()) textDefs.push_back(td);
            }
        }
        // ----------------------------------------------------------------
        // water <x> <height> <z>
        else if (cmd == "water") {
            if (tokens.size() >= 4) {
                WaterDef wd;
                wd.x      = std::stof(tokens[1]);
                wd.height = std::stof(tokens[2]);
                wd.z      = std::stof(tokens[3]);
                waterDefs.push_back(wd);
            }
        }
        // ----------------------------------------------------------------
        // animated_character <path> <x> <y|terrain[+N]> <z> [scale=F] [rot=RX,RY,RZ]
        //   path  — file path relative to src/Resources/Tutorial/ (incl. extension)
        //   x y z — world position (y supports "terrain[+N]" snap)
        //   scale — optional uniform scale (default 1.0)
        //   rot   — optional Euler rotation in degrees, comma-separated (default 0,0,0)
        //           Use rot=-90,0,0 to stand up a model that loads on its belly (Z-up FBX).
        else if (cmd == "animated_character") {
            if (tokens.size() >= 5) {
                AnimCharDef ac;
                ac.path = tokens[1];
                ac.x = std::stof(tokens[2]);
                float yo = 0.0f;
                ac.y = parseY(tokens[3], yo);
                if (ac.y == kSnapY) { ac.snapY = true; ac.yOffset = yo; }
                ac.z = std::stof(tokens[4]);
                for (size_t i = 5; i < tokens.size(); ++i) {
                    auto v = optVal(tokens[i], "scale");
                    if (!v.empty()) { ac.scale = std::stof(v); continue; }
                    v = optVal(tokens[i], "rot");
                    if (!v.empty()) {
                        // parse "RX,RY,RZ"
                        std::istringstream rs(v);
                        std::string comp;
                        float* dst[3] = {&ac.rx, &ac.ry, &ac.rz};
                        int ci = 0;
                        while (std::getline(rs, comp, ',') && ci < 3)
                            try { *dst[ci++] = std::stof(comp); } catch (...) {}
                    }
                }
                animCharDefs.push_back(ac);
            }
        }
        else if (cmd != "#") {
            std::cerr << "[SceneLoader] Unknown command '" << cmd
                      << "' at line " << lineNo << std::endl;
        }
    }
    file.close();

    // -----------------------------------------------------------------------
    // Pass 2: load terrain
    // -----------------------------------------------------------------------

    auto backgroundTex = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/grass")->getId());
    auto rTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/dirt")->getId());
    auto gTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/blueflowers")->getId());
    auto bTex          = new TerrainTexture(loader->loadTexture("MultiTextureTerrain/brickroad")->getId());
    auto texturePack   = new TerrainTexturePack(backgroundTex, rTex, gTex, bTex);
    auto blendMap      = new TerrainTexture(
        loader->loadTexture("MultiTextureTerrain/" + terrainDef.blendmapFile)->getId());

    // If the config did not specify any tiles, provide the default four tiles
    if (tileDefs.empty()) {
        tileDefs = {
            { 0, -1, true },
            {-1, -1, false },
            { 0,  0, false },
            {-1,  0, false },
        };
    }
    // Ensure exactly one primary tile (first tile if none marked)
    bool hasPrimary = false;
    for (auto& t : tileDefs) if (t.primary) { hasPrimary = true; break; }
    if (!hasPrimary) tileDefs[0].primary = true;

    for (auto& td : tileDefs) {
        auto t = new Terrain(td.gridX, td.gridZ, loader, texturePack, blendMap,
                             terrainDef.heightmapFile);
        allTerrains.push_back(t);
        if (td.primary) primaryTerrain = t;
    }

    // -----------------------------------------------------------------------
    // Pass 3: load models concurrently
    // -----------------------------------------------------------------------

    // model alias → (TexturedModel*, RawBoundingBox*)
    struct LoadedModel {
        TexturedModel*   model = nullptr;
        RawBoundingBox*  bbox  = nullptr;
    };
    std::map<std::string, LoadedModel> modelMap;

    // parallel load: one thread per model
    struct RawLoad {
        ModelData       data;
        BoundingBoxData bbData;
    };
    std::vector<RawLoad> rawLoads(modelDefs.size());

    {
        auto f = [](ModelData* pData, BoundingBoxData* pBb, const std::string& file) {
            *pData = OBJLoader::loadObjModel(file);
            *pBb   = OBJLoader::loadBoundingBox(*pData, ClickBoxTypes::BOX, BoundTypes::AABB);
        };
        std::vector<std::thread> threads;
        for (size_t i = 0; i < modelDefs.size(); ++i) {
            threads.emplace_back(f, &rawLoads[i].data, &rawLoads[i].bbData,
                                 modelDefs[i].objFile);
        }
        for (auto& t : threads) t.join();
    }

    // upload to GPU
    for (size_t i = 0; i < modelDefs.size(); ++i) {
        const auto& md = modelDefs[i];
        LoadedModel lm;
        lm.bbox = loader->loadToVAO(rawLoads[i].bbData);

        ModelTexture* tex;
        if (md.transparent || md.fakeLighting) {
            tex = new ModelTexture(md.textureFile, PNG, md.transparent, md.fakeLighting,
                                   Material{ md.shininess, md.reflectivity });
        } else {
            tex = new ModelTexture(md.textureFile, PNG,
                                   Material{ md.shininess, md.reflectivity });
        }
        if (md.atlasRows > 1) tex->setNumberOfRows(md.atlasRows);

        lm.model = new TexturedModel(loader->loadToVAO(rawLoads[i].data), tex);
        modelMap[md.alias] = lm;
    }

    // -----------------------------------------------------------------------
    // Pass 4: lights
    // -----------------------------------------------------------------------
    for (auto& ld : lightDefs) {
        float yVal = ld.y;
        if (ld.snapY && primaryTerrain) {
            yVal = primaryTerrain->getHeightOfTerrain(ld.x, ld.z) + ld.yOffset;
        }
        if (ld.directional) {
            lights.push_back(new Light(
                glm::vec3(ld.x, yVal, ld.z),
                glm::vec3(ld.r, ld.g, ld.b),
                Lighting{
                    .ambient  = glm::vec3(0.2f, 0.2f, 0.2f),
                    .diffuse  = glm::vec3(0.5f, 0.5f, 0.5f),
                    .constant = Light::kDirectional,
                }));
        } else {
            auto d = LightUtil::AttenuationDistance(static_cast<int>(ld.attenDist));
            Lighting l{ glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.5f, 0.5f, 0.5f),
                        d.x, d.y, d.z };
            lights.push_back(new Light(glm::vec3(ld.x, yVal, ld.z),
                                       glm::vec3(ld.r, ld.g, ld.b), l));
        }
    }

    // -----------------------------------------------------------------------
    // Pass 5: fixed entities
    // -----------------------------------------------------------------------
    for (auto& ed : entityDefs) {
        auto it = modelMap.find(ed.alias);
        if (it == modelMap.end()) {
            std::cerr << "[SceneLoader] entity references unknown model alias '"
                      << ed.alias << "'\n";
            continue;
        }
        auto& lm = it->second;
        float yVal = ed.y;
        if (ed.snapY && primaryTerrain)
            yVal = primaryTerrain->getHeightOfTerrain(ed.x, ed.z);

        entities.push_back(new Entity(
            lm.model,
            new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId()),
            glm::vec3(ed.x, yVal, ed.z),
            glm::vec3(ed.rx, ed.ry, ed.rz),
            ed.scale));
    }

    // -----------------------------------------------------------------------
    // Pass 6: random entities
    // -----------------------------------------------------------------------
    for (auto& rd : randomDefs) {
        auto it = modelMap.find(rd.alias);
        if (it == modelMap.end()) {
            std::cerr << "[SceneLoader] random references unknown model alias '"
                      << rd.alias << "'\n";
            continue;
        }
        auto& lm = it->second;
        for (int i = 0; i < rd.count; ++i) {
            glm::vec3 pos = randomPosition(primaryTerrain);
            glm::vec3 rot = randomRotation();
            float     sc  = randomScale(rd.scaleMin, rd.scaleMax);
            if (rd.useAtlas) {
                int idx = (rand() % 4) + 1;   // atlas row 1-4
                entities.push_back(new Entity(lm.model,
                    new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId()),
                    idx, pos, rot, sc));
            } else {
                entities.push_back(new Entity(lm.model,
                    new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId()),
                    pos, rot, sc));
            }
        }
    }

    // -----------------------------------------------------------------------
    // Pass 7: assimp entities
    // -----------------------------------------------------------------------
    for (auto& ad : assimpDefs) {
        auto* mesh = new AssimpMesh(ad.path);
        auto  bbData  = OBJLoader::loadBoundingBox(mesh, ClickBoxTypes::BOX, BoundTypes::AABB);
        auto* rawBb   = loader->loadToVAO(bbData);
        glm::vec3 pos = ad.randomPos
            ? randomPosition(primaryTerrain, ad.yOffset)
            : glm::vec3(ad.x, ad.y, ad.z);
        float sc = randomScale(ad.scaleMin, ad.scaleMax);
        scenes.push_back(new AssimpEntity(mesh,
            new BoundingBox(rawBb, BoundingBoxIndex::genUniqueId()),
            pos, randomRotation(), sc));
    }

    // -----------------------------------------------------------------------
    // Pass 8: player
    // -----------------------------------------------------------------------
    if (hasPlayer) {
        auto it = modelMap.find(playerDef.alias);
        if (it != modelMap.end()) {
            auto& lm = it->second;
            player = new Player(
                lm.model,
                new BoundingBox(lm.bbox, BoundingBoxIndex::genUniqueId()),
                glm::vec3(playerDef.x, playerDef.y, playerDef.z),
                glm::vec3(playerDef.rx, playerDef.ry, playerDef.rz),
                playerDef.scale);
            InteractiveModel::setInteractiveBox(player);
            entities.push_back(player);
            playerCamera = new PlayerCamera(player);
        } else {
            std::cerr << "[SceneLoader] player references unknown model alias '"
                      << playerDef.alias << "'\n";
        }
    }

    // -----------------------------------------------------------------------
    // Pass 9: GUI textures
    // -----------------------------------------------------------------------
    for (auto& gd : guiDefs) {
        auto* tex = loader->loadTexture(gd.textureFile);
        guis.push_back(new GuiTexture(tex->getId(),
                                      glm::vec2(gd.x, gd.y),
                                      glm::vec2(gd.w, gd.h)));
    }

    // -----------------------------------------------------------------------
    // Pass 10: water tiles
    // -----------------------------------------------------------------------
    for (auto& wd : waterDefs) {
        waterTiles.emplace_back(wd.x, wd.height, wd.z);
    }

    // -----------------------------------------------------------------------
    // Pass 11: text overlays
    //
    // Requires TextMaster::init(loader) to have been called by the caller
    // before SceneLoader::load() so that GUIText constructors can register
    // themselves into TextMaster's batch map.
    // -----------------------------------------------------------------------
    if (!textDefs.empty()) {
        // A single FontModel (dynamic VAO/VBO) is shared by all texts.
        FontModel* sharedFontModel = loader->loadFontVAO();

        // Cache loaded FontType objects by (name, size) to avoid re-loading
        // the same FreeType face multiple times.  Stored as heap-allocated
        // pointers so they outlive this function and remain valid for the
        // lifetime of the GUIText objects that reference them.
        std::map<std::pair<std::string,int>, FontType*> fontCache;

        for (auto& td : textDefs) {
            auto key = std::make_pair(td.fontName, td.fontSize);
            auto it = fontCache.find(key);
            if (it == fontCache.end()) {
                fontCache.emplace(key, new FontType(TextMeshData::loadFont(td.fontName, td.fontSize)));
                it = fontCache.find(key);
            }
            FontType* ft = it->second;

            // Font is loaded at td.fontSize pixels, so scale=1.0 renders at native size.
            // maxWidth is a screen-width fraction; convert to pixels for Line comparison.
            auto* guiText = new GUIText(
                td.message,
                1.0f,
                sharedFontModel,
                ft,
                glm::vec2(td.x, td.y),
                Color(td.r, td.g, td.b),
                td.maxWidth * static_cast<float>(DisplayManager::Width()),
                td.centered);
            texts.push_back(guiText);
        }
    }

    // -----------------------------------------------------------------------
    // Pass 12: animated characters
    // -----------------------------------------------------------------------

    // Helper: normalize clip name to a known state name for common variants.
    // Exact case-insensitive match for the four canonical names is tried first,
    // then substring match to handle "Walking"→"Walk", "Running"→"Run", etc.
    auto normalizeClipName = [](const std::string& raw) -> std::string {
        static const char* known[] = {"Idle", "Walk", "Run", "Jump", nullptr};

        // 1) Exact case-insensitive match
        for (int i = 0; known[i]; ++i) {
            if (raw.size() == std::strlen(known[i])) {
                bool same = true;
                for (size_t k = 0; k < raw.size(); ++k)
                    same = same && (std::tolower((unsigned char)raw[k]) ==
                                    std::tolower((unsigned char)known[i][k]));
                if (same) return known[i];
            }
        }

        // 2) Substring match — handles "Walking"→"Walk", "Running"→"Run",
        //    "Idle_02"→"Idle", "Jump_Start"→"Jump", "Armature|walk"→"Walk", etc.
        std::string lower(raw);
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("idle") != std::string::npos) return "Idle";
        if (lower.find("walk") != std::string::npos) return "Walk";
        if (lower.find("run")  != std::string::npos) return "Run";
        if (lower.find("jump") != std::string::npos) return "Jump";

        // 3) Unknown clip: capitalize the first letter and keep the rest.
        std::string out = raw;
        if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
        return out;
    };

    for (auto& ac : animCharDefs) {
        std::string absPath = FileSystem::Path("/src/Resources/Tutorial/" + ac.path);
        AnimatedModel* animModel = AnimationLoader::load(absPath);
        if (!animModel) {
            std::cerr << "[SceneLoader] Failed to load animated_character: " << absPath << "\n";
            continue;
        }

        float yVal = ac.y;
        if (ac.snapY && primaryTerrain)
            yVal = primaryTerrain->getHeightOfTerrain(ac.x, ac.z) + ac.yOffset;

        auto* controller = new AnimationController();

        // Register each clip under its normalized state name.
        for (auto& clip : animModel->clips) {
            std::string stateName = normalizeClipName(clip.name);
            controller->addState(stateName, &clip);
        }

        // Set up default transitions only when the known states exist.
        auto hasState = [&](const std::string& s) -> bool {
            for (const auto& clip : animModel->clips)
                if (normalizeClipName(clip.name) == s) return true;
            return false;
        };

        // NOTE: transitions are wired with real keyboard conditions by
        // MainGuiLoop::main() after load.  Do not add no-op placeholders here
        // to avoid accumulating duplicate transition entries.

        // Start playback on the first available state (prefer "Idle").
        // If there is no Idle clip, leave the controller with no active state so the
        // character stands in bind-pose until the player provides movement input.
        // The MainGuiLoop's setupDefaultTransitions call will add the "" → Walk/Run
        // transitions that allow animation to start on the first key press.
        if (hasState("Idle"))
            controller->setState("Idle");

        auto* ae       = new AnimatedEntity();
        ae->model      = animModel;
        ae->controller = controller;
        ae->position   = glm::vec3(ac.x, yVal, ac.z);
        ae->rotation   = glm::vec3(ac.rx, ac.ry, ac.rz);
        ae->scale      = ac.scale;

        animatedEntities.push_back(ae);

        // Log clip names so users can see exactly what animations the model contains
        // and which state name each clip was mapped to.
        std::cout << "[SceneLoader] Loaded animated_character '" << ac.path
                  << "': " << animModel->clips.size() << " clip(s), "
                  << animModel->skeleton.getBoneCount() << " bone(s).\n";
        for (const auto& clip : animModel->clips) {
            std::string sn = normalizeClipName(clip.name);
            std::cout << "[SceneLoader]   clip '" << clip.name
                      << "' → state '" << sn << "'\n";
        }
    }

    // -----------------------------------------------------------------------
    // Final status log
    // -----------------------------------------------------------------------
    std::cout << "[SceneLoader] Scene loaded: "
              << entities.size()        << " entities, "
              << scenes.size()          << " assimp scenes, "
              << lights.size()          << " lights, "
              << allTerrains.size()     << " terrain tiles, "
              << guis.size()            << " GUI textures, "
              << texts.size()           << " text overlays, "
              << waterTiles.size()      << " water tiles, "
              << animatedEntities.size()<< " animated characters." << std::endl;

    return true;
}
