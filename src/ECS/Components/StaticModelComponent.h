#ifndef ECS_STATICMODELCOMPONENT_H
#define ECS_STATICMODELCOMPONENT_H

class TexturedModel;
class BoundingBox;

/// ECS component that holds a static (non-skinned) renderable mesh.
/// Replaces the legacy `new Entity(...)` + `entities_` vector pattern for
/// scene-level fixed geometry loaded from scene.json's "entities" array.
///
/// Entities tagged with this component are rendered by RenderSystem via
/// `registry.view<StaticModelComponent, TransformComponent>()`.
/// No legacy `std::vector<Entity*>` needed.
struct StaticModelComponent {
    TexturedModel* model        = nullptr;  ///< Textured mesh (shared; owned by Loader)
    BoundingBox*   boundingBox  = nullptr;  ///< Owned by this component; delete in shutdown
    int            textureIndex = 0;        ///< Texture atlas row (atlas-mapped textures)
};

#endif // ECS_STATICMODELCOMPONENT_H
