# Characters Model Directory

## Expected files

| File | Description |
|------|-------------|
| `characters.glb` | Skinned character mesh with Walk, Run, and Idle animations. |

## How to add `characters.glb`

1. Open your character rig in Blender (or another DCC tool).
2. Export → glTF 2.0 (.glb) with the following settings:
   - **Format**: glTF Binary (.glb)
   - **Transform**: Y Up
   - **Animation**: NLA Tracks or Active Actions
3. Place the exported file here:
   ```
   src/Resources/Models/Characters/characters.glb
   ```
4. In `src/Resources/scene.json`, update the `animated_characters` entry:
   ```json
   { "path": "Characters/characters.glb", ... }
   ```
   (Remove the `_MISSING_` prefix that was added by `migrate_models.sh` to
   suppress the load error while the file was absent.)

## Naming conventions

The animation clips expected by `AnimationController` (set up in
`Engine::loadScene` / `EntityFactory`) are:

| Clip name | Trigger condition |
|-----------|-------------------|
| `Idle`    | No movement       |
| `Walk`    | Low speed         |
| `Run`     | High speed        |
