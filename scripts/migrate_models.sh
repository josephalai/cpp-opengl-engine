#!/usr/bin/env bash
# scripts/migrate_models.sh
#
# Migrates model assets from src/Resources/Tutorial/ to an organised layout
# under src/Resources/Models/ and updates all JSON / C++ references.
#
# Target layout:
#   src/Resources/Models/Objects/    — stall, lamp, box, crate, drone, exampleOBJ
#   src/Resources/Models/Nature/     — tree, fluffy-tree, grass, grassModel,
#                                      fern, tree2, tree3, Tree1, lowPolyTree
#   src/Resources/Models/Characters/ — person, dragon, bunny, stanfordBunny
#   src/Resources/Models/Backpack/   — (Backpack directory contents)
#
# After migration the scene.json "models" array obj fields, animated_characters
# paths, and prefab model.obj fields are updated to use the new categorised
# sub-paths (Objects/, Nature/, Characters/, Backpack/).
#
# Usage:
#   ./scripts/migrate_models.sh          # live run
#   ./scripts/migrate_models.sh --dry-run # preview only, no changes made
#
# Idempotent: running twice produces no extra changes.

set -euo pipefail

# ---------------------------------------------------------------------------
# Argument handling
# ---------------------------------------------------------------------------
DRY_RUN=false
for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=true ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RESOURCES="$ROOT/src/Resources"
TUTORIAL="$RESOURCES/Tutorial"
MODELS="$RESOURCES/Models"
SCENE_JSON="$RESOURCES/scene.json"
NPCS_JSON="$RESOURCES/npcs.json"
PREFABS_DIR="$RESOURCES/prefabs"

# ---------------------------------------------------------------------------
# Colour helpers
# ---------------------------------------------------------------------------
if [ -t 1 ]; then
    RED='\033[0;31m'; YELLOW='\033[1;33m'; GREEN='\033[0;32m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; YELLOW=''; GREEN=''; CYAN=''; BOLD=''; RESET=''
fi

info()  { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${RESET}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
err()   { echo -e "${RED}[ERR ]${RESET}  $*" >&2; }
step()  { echo -e "\n${BOLD}=== $* ===${RESET}"; }
dryrun(){ echo -e "${YELLOW}[DRY ]${RESET}  (would) $*"; }

if $DRY_RUN; then
    warn "DRY-RUN mode — no files or JSON will be modified."
fi

# ---------------------------------------------------------------------------
# Model category map
# Keys: bare model name (without extension)
# Values: destination sub-directory under Models/
# ---------------------------------------------------------------------------
declare -A MODEL_CATEGORY
MODEL_CATEGORY["stall"]="Objects"
MODEL_CATEGORY["lamp"]="Objects"
MODEL_CATEGORY["box"]="Objects"
MODEL_CATEGORY["crate"]="Objects"
MODEL_CATEGORY["drone"]="Objects"
MODEL_CATEGORY["exampleOBJ"]="Objects"
MODEL_CATEGORY["tree"]="Nature"
MODEL_CATEGORY["fluffy-tree"]="Nature"
MODEL_CATEGORY["grass"]="Nature"
MODEL_CATEGORY["grassModel"]="Nature"
MODEL_CATEGORY["fern"]="Nature"
MODEL_CATEGORY["tree2"]="Nature"
MODEL_CATEGORY["tree3"]="Nature"
MODEL_CATEGORY["Tree1"]="Nature"
MODEL_CATEGORY["lowPolyTree"]="Nature"
MODEL_CATEGORY["person"]="Characters"
MODEL_CATEGORY["dragon"]="Characters"
MODEL_CATEGORY["bunny"]="Characters"
MODEL_CATEGORY["stanfordBunny"]="Characters"

# ---------------------------------------------------------------------------
# Helper: move a single model file (git mv if tracked, plain mv otherwise)
# Only moves .obj / .glb / .fbx / .dae files (never textures).
# ---------------------------------------------------------------------------
move_model() {
    local src="$1"
    local dst_dir="$2"
    local filename
    filename="$(basename "$src")"
    local ext="${filename##*.}"

    # Only model formats — skip textures
    case "${ext,,}" in
        obj|glb|fbx|dae) ;;
        *) return 0 ;;
    esac

    local dst="$dst_dir/$filename"

    if [ ! -f "$src" ]; then
        warn "Source not found, skipping: $src"
        return 0
    fi
    if [ -f "$dst" ]; then
        ok "Already at destination: $dst"
        return 0
    fi

    if $DRY_RUN; then
        dryrun "mv $src -> $dst"
        return 0
    fi

    mkdir -p "$dst_dir"

    # Use git mv if the file is tracked, otherwise plain mv
    if git -C "$ROOT" ls-files --error-unmatch "$src" &>/dev/null; then
        git -C "$ROOT" mv "$src" "$dst"
        ok "git mv: $(basename "$src") -> ${dst_dir#$ROOT/}"
    else
        mv "$src" "$dst"
        ok "mv:     $(basename "$src") -> ${dst_dir#$ROOT/}"
    fi
}

# ---------------------------------------------------------------------------
# Helper: move an entire directory (only model files inside)
# ---------------------------------------------------------------------------
move_dir_models() {
    local src_dir="$1"
    local dst_dir="$2"
    if [ ! -d "$src_dir" ]; then
        warn "Source directory not found, skipping: $src_dir"
        return 0
    fi
    find "$src_dir" -maxdepth 1 -type f | while read -r f; do
        move_model "$f" "$dst_dir"
    done
}

# ---------------------------------------------------------------------------
# Helper: sed-in-place (portable macOS / Linux)
# ---------------------------------------------------------------------------
sed_inplace() {
    local pattern="$1"
    local file="$2"
    if $DRY_RUN; then
        dryrun "sed '$pattern' $file"
        return 0
    fi
    # BSD sed needs an extension argument (even empty string)
    if sed --version &>/dev/null; then
        # GNU sed
        sed -i "$pattern" "$file"
    else
        # BSD sed (macOS)
        sed -i '' "$pattern" "$file"
    fi
}

# ---------------------------------------------------------------------------
# Helper: update model obj references in a JSON file using the category map.
# Rewrites bare model names in "obj" fields to "Category/name" paths.
# Also updates animated_characters "path" entries from Tutorial-relative
# paths to Models-relative paths.
# Idempotent: already-updated paths are left untouched.
# ---------------------------------------------------------------------------
update_json_model_paths() {
    local file="$1"
    if [ ! -f "$file" ]; then return 0; fi

    if $DRY_RUN; then
        dryrun "update model paths in $file"
        return 0
    fi

    python3 - "$file" <<'PYEOF'
import json, sys, os

fpath = sys.argv[1]

# Category map: bare model name -> sub-directory
CATEGORY = {
    "stall":        "Objects",
    "lamp":         "Objects",
    "box":          "Objects",
    "crate":        "Objects",
    "drone":        "Objects",
    "exampleOBJ":   "Objects",
    "tree":         "Nature",
    "fluffy-tree":  "Nature",
    "grass":        "Nature",
    "grassModel":   "Nature",
    "fern":         "Nature",
    "tree2":        "Nature",
    "tree3":        "Nature",
    "Tree1":        "Nature",
    "lowPolyTree":  "Nature",
    "person":       "Characters",
    "dragon":       "Characters",
    "bunny":        "Characters",
    "stanfordBunny":"Characters",
    "backpack":     "Backpack",
}

with open(fpath) as f:
    data = json.load(f)

changed = False

def update_obj_field(obj_val):
    """Prefix a bare model name with its category.  Already-prefixed values
    (contain '/') and unknown names are left untouched."""
    if '/' in obj_val:
        return obj_val  # already updated
    cat = CATEGORY.get(obj_val)
    if cat:
        return f"{cat}/{obj_val}"
    return obj_val

# --- scene.json / npcs.json: models array ---
for m in data.get("models", []):
    if "obj" in m:
        new_val = update_obj_field(m["obj"])
        if new_val != m["obj"]:
            m["obj"] = new_val
            changed = True

# --- scene.json: animated_characters paths ---
# The engine currently resolves these relative to Tutorial/, e.g.
#   "Characters/characters.glb" -> Tutorial/Characters/characters.glb
# After migration, the engine path base will be Models/, so the JSON
# value stays as "Characters/characters.glb".
# If the path starts with "Tutorial/", strip it (idempotency guard).
for ac in data.get("animated_characters", []):
    if "path" in ac and ac["path"].startswith("Tutorial/"):
        ac["path"] = ac["path"][len("Tutorial/"):]
        changed = True

# --- prefab JSONs: model.obj field ---
model_block = data.get("model")
if isinstance(model_block, dict) and "obj" in model_block:
    new_val = update_obj_field(model_block["obj"])
    if new_val != model_block["obj"]:
        model_block["obj"] = new_val
        changed = True

if changed:
    with open(fpath, 'w') as f:
        json.dump(data, f, indent=2)
        f.write('\n')
    print(f"  updated: {os.path.basename(fpath)}")
else:
    print(f"  no changes needed: {os.path.basename(fpath)}")
PYEOF
}

# ===========================================================================
# STEP 1: Create target directories
# ===========================================================================
step "1. Creating target directories"
for d in Objects Nature Characters Backpack; do
    tgt="$MODELS/$d"
    if $DRY_RUN; then
        dryrun "mkdir -p $tgt"
    else
        mkdir -p "$tgt"
        ok "Directory ready: ${tgt#$ROOT/}"
    fi
done

# ===========================================================================
# STEP 2: Move model files
# ===========================================================================
step "2. Moving model files from Tutorial/"

if [ ! -d "$TUTORIAL" ]; then
    warn "src/Resources/Tutorial/ not found — nothing to migrate."
    warn "This is expected if migration has already been completed."
else
    # Objects
    for model in stall lamp box crate drone exampleOBJ; do
        move_model "$TUTORIAL/$model.obj" "$MODELS/Objects"
    done

    # Nature
    for model in tree "fluffy-tree" grass grassModel fern tree2 tree3 Tree1 lowPolyTree; do
        move_model "$TUTORIAL/$model.obj" "$MODELS/Nature"
    done

    # Characters
    for model in person dragon bunny stanfordBunny; do
        move_model "$TUTORIAL/$model.obj" "$MODELS/Characters"
    done

    # Backpack directory
    if [ -d "$TUTORIAL/Backpack" ]; then
        move_dir_models "$TUTORIAL/Backpack" "$MODELS/Backpack"
    fi

    # Remove Tutorial directory if now empty (non-dry-run only)
    if ! $DRY_RUN; then
        if [ -d "$TUTORIAL" ] && [ -z "$(find "$TUTORIAL" -type f 2>/dev/null)" ]; then
            rm -rf "$TUTORIAL"
            ok "Removed empty Tutorial directory"
        fi
    fi
fi

# ===========================================================================
# STEP 3: Handle missing characters.glb
# ===========================================================================
step "3. Handling missing characters.glb"

CHARS_GLB="$MODELS/Characters/characters.glb"
CHARS_README="$MODELS/Characters/README.md"

if [ -f "$CHARS_GLB" ]; then
    ok "characters.glb already present."
else
    warn "characters.glb is MISSING from the repository."
    warn "The animated_characters entry in scene.json references:"
    warn "  Characters/characters.glb"
    warn ""
    warn "You must supply a valid .glb file (e.g. exported from Blender) and"
    warn "place it at:"
    warn "  src/Resources/Models/Characters/characters.glb"
    warn ""
    warn "The animated_characters entry in scene.json has been prefixed with"
    warn "  _MISSING_ to prevent a fatal load error until the file is added."

    # Prefix the path in scene.json so the loader skips it gracefully
    if grep -q '"Characters/characters.glb"' "$SCENE_JSON" 2>/dev/null; then
        sed_inplace 's|"Characters/characters.glb"|"_MISSING_Characters/characters.glb"|g' \
            "$SCENE_JSON"
        ok "scene.json animated_characters path prefixed with _MISSING_"
    fi

    # Create README in the Characters directory
    if $DRY_RUN; then
        dryrun "create $CHARS_README"
    else
        mkdir -p "$MODELS/Characters"
        cat > "$CHARS_README" <<'README'
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
README
        ok "Created $CHARS_README"
    fi
fi

# ===========================================================================
# STEP 4: Update JSON file references
# Update "obj" fields in the models array and prefab JSONs from bare model
# names (e.g. "stall") to categorised paths (e.g. "Objects/stall").
# ===========================================================================
step "4. Updating JSON model path references"

info "Updating scene.json ..."
update_json_model_paths "$SCENE_JSON"

info "Updating npcs.json ..."
update_json_model_paths "$NPCS_JSON"

for prefab in "$PREFABS_DIR"/*.json; do
    [ -f "$prefab" ] || continue
    info "Updating prefab: $(basename "$prefab") ..."
    update_json_model_paths "$prefab"
done

# ===========================================================================
# STEP 5: Report C++ source files with hard-coded Tutorial/ paths
# ===========================================================================
step "5. C++ source files referencing Tutorial/ (manual review required)"

HARDCODED=()
while IFS= read -r -d '' f; do
    HARDCODED+=("$f")
done < <(grep -rl --include="*.cpp" --include="*.h" \
    '"Tutorial/' "$ROOT/src" 2>/dev/null -Z || true)

if [ ${#HARDCODED[@]} -eq 0 ]; then
    ok "No C++ source files contain hard-coded Tutorial/ paths."
else
    warn "The following C++ files contain hard-coded Tutorial/ path strings."
    warn "Update them to use Models/Objects/, Models/Nature/, or Models/Characters/ as appropriate:"
    for f in "${HARDCODED[@]}"; do
        echo "    ${f#$ROOT/}"
        grep -n '"Tutorial/' "$f" | sed 's/^/        /'
    done
fi

# ===========================================================================
# Done
# ===========================================================================
echo ""
if $DRY_RUN; then
    echo -e "${YELLOW}${BOLD}DRY-RUN complete — no files were modified.${RESET}"
    echo -e "Run without --dry-run to apply changes."
else
    echo -e "${GREEN}${BOLD}Migration complete.${RESET}"
fi
