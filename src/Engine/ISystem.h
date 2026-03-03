// src/Engine/ISystem.h
// Base interface for all engine subsystems.
// Each subsystem has a well-defined lifecycle: init → update(dt) → shutdown.

#ifndef ENGINE_ISYSTEM_H
#define ENGINE_ISYSTEM_H

class ISystem {
public:
    virtual ~ISystem() = default;

    /// Called once during engine initialisation (GL context is active).
    virtual void init() = 0;

    /// Called every frame with the elapsed time in seconds.
    virtual void update(float deltaTime) = 0;

    /// Called once on shutdown; release GPU/CPU resources here.
    virtual void shutdown() = 0;
};

#endif // ENGINE_ISYSTEM_H
