//
// Created by Joseph Alai on 6/30/21.
//
#if __APPLE__
#define RETINA_NUMBER 2
#else
#define RETINA_NUMBER 1
#endif

#define RETINA_DIVIDE(DIM)((DIM)/RETINA_NUMBER)
#define RETINA_SCALE(DIM)((DIM)*RETINA_NUMBER)

#ifndef ENGINE_DISPLAYMANAGER_H
#define ENGINE_DISPLAYMANAGER_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB

#include <GLFW/glfw3.h>

class DisplayManager {
public:

    static GLFWwindow *window;

    static float delta;

    static float lastFrameTime;

    static int samplers;

    static bool antiAliasingEnabled;

    static int createDisplay();

    static void updateDisplay();

    static bool stayOpen();

    static void closeDisplay();

    static void uniformMovement();

    static float getFrameTimeSeconds();

    static GLint &Width();

    static GLint &Height();

    static GLint &FboWidth();

    static GLint &FboHeight();

    static bool resetMouse;

    static void framebuffer_size_callback(GLFWwindow *window, int width, int height);

private:

    static void updatePerspective(int width, int height);

    static GLint SRC_WIDTH;

    static GLint SRC_HEIGHT;

    static GLint FBO_WIDTH;

    static GLint FBO_HEIGHT;

};

#endif //ENGINE_DISPLAYMANAGER_H
