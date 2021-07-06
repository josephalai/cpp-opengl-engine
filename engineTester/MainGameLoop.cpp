//
// Created by Joseph Alai on 7/6/21.
//

#define PRINTXYZ(VEC){printf("%f, %f, %f\n", VEC[0], VEC[1], VEC[2]);};

#include "MainGameLoop.h"
#include "../toolbox/FileSystem.h"
#include "../toolbox/Utils.h"
#include "../renderEngine/DisplayManager.h"
#include "../renderEngine/Loader.h"
#include "../renderEngine/EntityRenderer.h"
#include "../renderEngine/ObjLoader.h"
#include "../renderEngine/MasterRenderer.h"

using namespace glm;

void MainGameLoop::main() {
    DisplayManager::createDisplay();

    Loader *loader;
    Camera *viewCamera;
    CameraInput *cameraInput;

    loader = new Loader();
    viewCamera = new Camera(glm::vec3(0.0f, 4.5f, 0.0f));
    cameraInput = new CameraInput(viewCamera);

    RawModel *grassModel, *treeModel, *stallModel, *dragonModel;
    ModelTexture *grassTexture, *treeTexture, *stallTexture, *dragonTexture;
    TexturedModel *staticGrass, *staticTree, *staticStall, *staticDragon;
    Light *light;

    light = new Light(glm::vec3(0.0, 4.5, -10.0f), glm::vec3(1, 1, 1));


    ModelData dragonData = OBJLoader::loadObjModel("/res/tut/dragon.obj");
    dragonModel = loader->loadToVAO(&dragonData);
    dragonTexture = new ModelTexture(FileSystem::Path("/res/tut/grassTexture.png"), PNG);
    staticDragon = new TexturedModel(dragonModel, dragonTexture);
    auto dragonEntity = new Entity(staticDragon, glm::vec3(0.0, 120.0, 80), glm::vec3(0.0f, 180.0f, 0.0f));

    ModelData grassData = OBJLoader::loadObjModel("/res/tut/grassModel.obj");
    grassModel = loader->loadToVAO(&grassData);
    grassTexture = new ModelTexture(FileSystem::Path("/res/tut/grassTexture.png"), PNG);
    staticGrass = new TexturedModel(grassModel, grassTexture);
    grassTexture->setHasTransparency(true);
    grassTexture->setUseFakeLighting(true);

    ModelData treeData = OBJLoader::loadObjModel("/res/tut/lowPolyTree.obj");
    treeModel = loader->loadToVAO(&treeData);
    treeTexture = new ModelTexture(FileSystem::Path("/res/tut/lowPolyTree.png"), PNG);
    staticTree = new TexturedModel(treeModel, treeTexture);

    ModelData stallData = OBJLoader::loadObjModel("/res/stall/stall.obj");
    stallModel = loader->loadToVAO(&stallData);
    stallTexture = new ModelTexture(FileSystem::Path("/res/stall/stallTexture.png"), PNG, Material{
            .ambient =  glm::vec3(1),
            .diffuse =  glm::vec3(1),
            .specular =  glm::vec3(0.3),
            .shininess = 32.0f});
    staticStall = new TexturedModel(stallModel, stallTexture);

    std::vector<Entity *> allEntities;
    allEntities.push_back(new Entity(staticStall, glm::vec3(1.0f, 0.0f, -82.4f), glm::vec3(0.0f, 180.0f, 0.0f)));

    for (int i = 0; i < 50; ++i) {
        float x = randomFloat() * 100 - 50;
        float y = randomFloat() * 0;
        float z = randomFloat() * -300;

        float rx, ry, rz, scale;
        rx = 0;
        ry = 0;
        rz = 0;
        glm::vec3 rot(rx, ry, rz);
        rot = rot * 180.0f;
        allEntities.push_back(new Entity(staticGrass, glm::vec3(x, y, z), rot));
        allEntities.push_back(new Entity(staticTree, glm::vec3(x * 1.5, y, z * 1.5), rot, randomFloat()));
    }

    Terrain *terrain, *terrain2;
    terrain = new Terrain(-1, -1, loader, new ModelTexture(FileSystem::Path("/res/grass.png"), PNG));
    terrain2 = new Terrain(0, -1, loader, new ModelTexture(FileSystem::Path("/res/grass.png"), PNG));

    MasterRenderer *renderer;
    renderer = new MasterRenderer(cameraInput);
    while (DisplayManager::stayOpen()) {
        // game logic
        renderer->processTerrain(terrain);
        renderer->processTerrain(terrain2);
        for (Entity *booth : allEntities) {
            renderer->processEntity(booth);
        }
        renderer->processEntity(dragonEntity);
        light->setPosition(light->getPosition() + glm::vec3(0.0, 0.01, -0.1f));
        renderer->render(light);
        dragonEntity->setPosition(light->getPosition());
        DisplayManager::updateDisplay();
    }

    renderer->cleanUp();
    loader->cleanUp();
    DisplayManager::closeDisplay();
}