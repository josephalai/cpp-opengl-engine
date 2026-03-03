// src/Animation/BoneBuffer.h
// GPU Uniform Buffer Object (UBO) for uploading the bone matrix palette.
// Replaces uploading individual bone matrices as separate uniforms, which
// does not scale past ~100 bones on most drivers.
//
// Usage:
//   BoneBuffer buf;
//   buf.init();                             // call once after GL context is ready
//   buf.upload(matrices);                   // call each frame before draw
//   buf.bind(BoneBuffer::kBindingPoint);    // bind UBO to the shader's block slot
//   glDrawElements(...);
//   buf.cleanup();                          // call on shutdown

#ifndef ENGINE_BONEBUFFER_H
#define ENGINE_BONEBUFFER_H

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <vector>
#include <glm/glm.hpp>

class BoneBuffer {
public:
    /// The UBO binding point shared between the C++ code and GLSL (BoneBlock).
    static constexpr GLuint kBindingPoint = 0;

    BoneBuffer() = default;

    /// Create and allocate the UBO (call once after the GL context exists).
    /// @param maxBones  capacity in matrices — must match MAX_BONES in the shader.
    void init(int maxBones = 100);

    /// Upload bone matrices to the UBO (GL_DYNAMIC_DRAW — safe to call every frame).
    /// Uploads at most maxBones matrices; extra entries are silently ignored.
    void upload(const std::vector<glm::mat4>& matrices);

    /// Bind the UBO to @p bindingPoint so the shader can read it.
    void bind(GLuint bindingPoint = kBindingPoint) const;

    /// Release the UBO.
    void cleanup();

    bool isInitialized() const { return ubo_ != 0; }

private:
    GLuint ubo_      = 0;
    int    maxBones_ = 0;
};

#endif // ENGINE_BONEBUFFER_H
