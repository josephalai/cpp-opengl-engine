# Missing characters.glb

This project expects an animated GLB at:

    src/Resources/Models/Characters/characters.glb

The migration script did not find that file in the repository, so the
`animated_characters` entry in `scene.json` was temporarily changed to:

    _MISSING_Characters/characters.glb

This is intentional so the engine can avoid a fatal load error until the
real asset is added.

## To fix

1. Add a valid GLB file at:
   `src/Resources/Models/Characters/characters.glb`

2. Then restore the path in `src/Resources/scene.json` from:

   `_MISSING_Characters/characters.glb`

   back to:

   `Characters/characters.glb`

3. Re-run the migration script if needed. It is intended to be idempotent.
