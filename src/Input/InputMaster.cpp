//
// Created by Joseph Alai on 7/26/21.
//
#include "InputMaster.h"
#include "../RenderEngine/DisplayManager.h"
#include "../Toolbox/Picker.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cctype>

bool InputMaster::pendingClick;

bool InputMaster::pendingButton;

ClickButtons InputMaster::lastClick;

KeyboardKeys InputMaster::lastButton;

std::vector<KeyboardKeys> InputMaster::pressedKeys;

std::vector<ClickButtons> InputMaster::clickedButtons;

double InputMaster::mouseX, InputMaster::mouseY;

double InputMaster::lastMouseX, InputMaster::lastMouseY;

float InputMaster::mouseDx, InputMaster::mouseDy;

std::unordered_map<std::string, int> InputMaster::actionMap_;

// ---------------------------------------------------------------------------
// Key-name string → GLFW key code mapping for controls.json parsing
// ---------------------------------------------------------------------------
static int keyNameToGLFW(const std::string& name) {
    static const std::unordered_map<std::string, int> kMap = {
        {"Space", GLFW_KEY_SPACE},
        {"Apostrophe", GLFW_KEY_APOSTROPHE},
        {"Comma", GLFW_KEY_COMMA},
        {"Minus", GLFW_KEY_MINUS},
        {"Period", GLFW_KEY_PERIOD},
        {"Slash", GLFW_KEY_SLASH},
        {"0", GLFW_KEY_0}, {"1", GLFW_KEY_1}, {"2", GLFW_KEY_2},
        {"3", GLFW_KEY_3}, {"4", GLFW_KEY_4}, {"5", GLFW_KEY_5},
        {"6", GLFW_KEY_6}, {"7", GLFW_KEY_7}, {"8", GLFW_KEY_8},
        {"9", GLFW_KEY_9},
        {"Semicolon", GLFW_KEY_SEMICOLON}, {"Equal", GLFW_KEY_EQUAL},
        {"A", GLFW_KEY_A}, {"B", GLFW_KEY_B}, {"C", GLFW_KEY_C},
        {"D", GLFW_KEY_D}, {"E", GLFW_KEY_E}, {"F", GLFW_KEY_F},
        {"G", GLFW_KEY_G}, {"H", GLFW_KEY_H}, {"I", GLFW_KEY_I},
        {"J", GLFW_KEY_J}, {"K", GLFW_KEY_K}, {"L", GLFW_KEY_L},
        {"M", GLFW_KEY_M}, {"N", GLFW_KEY_N}, {"O", GLFW_KEY_O},
        {"P", GLFW_KEY_P}, {"Q", GLFW_KEY_Q}, {"R", GLFW_KEY_R},
        {"S", GLFW_KEY_S}, {"T", GLFW_KEY_T}, {"U", GLFW_KEY_U},
        {"V", GLFW_KEY_V}, {"W", GLFW_KEY_W}, {"X", GLFW_KEY_X},
        {"Y", GLFW_KEY_Y}, {"Z", GLFW_KEY_Z},
        {"LeftBracket", GLFW_KEY_LEFT_BRACKET},
        {"Backslash", GLFW_KEY_BACKSLASH},
        {"RightBracket", GLFW_KEY_RIGHT_BRACKET},
        {"GraveAccent", GLFW_KEY_GRAVE_ACCENT},
        {"Escape", GLFW_KEY_ESCAPE}, {"Enter", GLFW_KEY_ENTER},
        {"Tab", GLFW_KEY_TAB}, {"Backspace", GLFW_KEY_BACKSPACE},
        {"Insert", GLFW_KEY_INSERT}, {"Delete", GLFW_KEY_DELETE},
        {"Right", GLFW_KEY_RIGHT}, {"Left", GLFW_KEY_LEFT},
        {"Down", GLFW_KEY_DOWN}, {"Up", GLFW_KEY_UP},
        {"PageUp", GLFW_KEY_PAGE_UP}, {"PageDown", GLFW_KEY_PAGE_DOWN},
        {"Home", GLFW_KEY_HOME}, {"End", GLFW_KEY_END},
        {"CapsLock", GLFW_KEY_CAPS_LOCK},
        {"F1", GLFW_KEY_F1}, {"F2", GLFW_KEY_F2}, {"F3", GLFW_KEY_F3},
        {"F4", GLFW_KEY_F4}, {"F5", GLFW_KEY_F5}, {"F6", GLFW_KEY_F6},
        {"F7", GLFW_KEY_F7}, {"F8", GLFW_KEY_F8}, {"F9", GLFW_KEY_F9},
        {"F10", GLFW_KEY_F10}, {"F11", GLFW_KEY_F11}, {"F12", GLFW_KEY_F12},
        {"LeftShift", GLFW_KEY_LEFT_SHIFT},
        {"LeftControl", GLFW_KEY_LEFT_CONTROL},
        {"LeftAlt", GLFW_KEY_LEFT_ALT},
        {"RightShift", GLFW_KEY_RIGHT_SHIFT},
        {"RightControl", GLFW_KEY_RIGHT_CONTROL},
        {"RightAlt", GLFW_KEY_RIGHT_ALT},
    };
    auto it = kMap.find(name);
    if (it != kMap.end()) return it->second;
    // Single uppercase-letter fallback (GLFW key codes match uppercase ASCII).
    if (name.size() == 1) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        return static_cast<int>(c);
    }
    return GLFW_KEY_UNKNOWN;
}

void InputMaster::loadBindings(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[InputMaster] Could not open " << path
                  << " — using default bindings.\n";
        return;
    }
    nlohmann::json root;
    try { file >> root; }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[InputMaster] JSON parse error in " << path
                  << ": " << e.what() << "\n";
        return;
    }
    if (root.contains("bindings") && root["bindings"].is_object()) {
        for (auto& [action, keyName] : root["bindings"].items()) {
            int code = keyNameToGLFW(keyName.get<std::string>());
            if (code != GLFW_KEY_UNKNOWN) {
                actionMap_[action] = code;
            } else {
                std::cerr << "[InputMaster] Unknown key name \""
                          << keyName.get<std::string>()
                          << "\" for action \"" << action << "\"\n";
            }
        }
    }
    std::cout << "[InputMaster] Loaded " << actionMap_.size()
              << " binding(s) from " << path << "\n";
}

bool InputMaster::isActionDown(const std::string& action) {
    auto it = actionMap_.find(action);
    if (it == actionMap_.end()) return false;
    return glfwGetKey(DisplayManager::window, it->second) == GLFW_PRESS;
}


void InputMaster::init() {
    glfwSetKeyCallback(DisplayManager::window, key_callback);
    glfwSetMouseButtonCallback(DisplayManager::window, mouse_callback);
}

bool InputMaster::hasPendingClick() {
    if (pendingClick) {
        return true;
    }
    return false;
}

bool InputMaster::hasPendingButton() {
    if (pendingButton) {
        return true;
    }
    return false;
}

bool InputMaster::buttonPressed(KeyboardKeys key) {
    if (lastButton == key) {
        return true;
    }
    return false;
}

bool InputMaster::mouseClicked(ClickButtons button) {
    if (lastClick == button) {
        return true;
    }
    return false;
}

int InputMaster::lastTyped() {
    return lastButton;
}

int InputMaster::lastClicked() {
    return lastClick;
}

void InputMaster::clearKeys() {
    lastButton = Unknown;
}

void InputMaster::setKey(KeyboardKeys key) {
    auto findKey = std::find(pressedKeys.begin(), pressedKeys.end(), key);
    if (findKey != pressedKeys.end()) {
        pressedKeys.erase(findKey);
        return;
    }
    pendingButton = true;
    lastButton = key;
}

void InputMaster::setClick(ClickButtons button) {
    auto findButton = std::find(clickedButtons.begin(), clickedButtons.end(), button);
    if (findButton != clickedButtons.end()) {
        clickedButtons.erase(findButton);
        return;
    }
    pendingClick = true;
    clickedButtons.push_back(button);
    lastClick = button;
}

void InputMaster::resetClick() {
    lastClick = Nothing;
}

void InputMaster::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {}
    setKey(static_cast<KeyboardKeys>(key));
}

void InputMaster::mouse_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS) {}
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS) {}
    setClick(static_cast<ClickButtons>(button));
}

void InputMaster::clearKey(KeyboardKeys key) {
    pressedKeys.erase(std::remove(pressedKeys.begin(), pressedKeys.end(), key), pressedKeys.end());
}

bool InputMaster::findKey(KeyboardKeys key) {
    if (std::find(pressedKeys.begin(), pressedKeys.end(), key) != pressedKeys.end()) {
        return true;
    }
    return false;
}

bool InputMaster::isKeyDown(KeyboardKeys key) {
    return glfwGetKey(DisplayManager::window, key) == GLFW_PRESS;
}

bool InputMaster::isMouseDown(ClickButtons click) {
    return glfwGetMouseButton(DisplayManager::window, click) == GLFW_PRESS;
}