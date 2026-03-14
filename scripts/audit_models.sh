#!/usr/bin/env bash
# scripts/audit_models.sh
# Audits model assets, textures, scene.json registrations, and prefab JSON
# files to detect missing files, case mismatches, and unregistered aliases.
#
# Usage: ./scripts/audit_models.sh [repo-root]
# Defaults to the directory containing this script's parent.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${1:-"$SCRIPT_DIR/.."}"
RESOURCES="$ROOT/src/Resources"
SCENE_JSON="$RESOURCES/scene.json"
PREFABS_DIR="$RESOURCES/prefabs"

# Colours (disabled if not a TTY)
if [ -t 1 ]; then
    RED='\033[0;31m'; YELLOW='\033[1;33m'; GREEN='\033[0;32m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; YELLOW=''; GREEN=''; CYAN=''; BOLD=''; RESET=''
fi

ISSUE_COUNT=0

record_issue() { (( ISSUE_COUNT++ )) || true; echo -e "${RED}  [ISSUE]${RESET} $*"; }
ok()           { echo -e "${GREEN}  [OK]${RESET}    $*"; }
info()         { echo -e "${CYAN}  $*${RESET}"; }
header()       { echo -e "\n${BOLD}$*${RESET}"; echo "$(echo "$*" | tr -c '\n' '-')"; }

# ───────────────────────────────────────────────────────────────────────────────
# 1. Model files on disk
# ───────────────────────────────────────────────────────────────────────────────
header "1. Model files on disk (src/Resources/)"
find "$RESOURCES" -type f \( -iname "*.obj" -o -iname "*.glb" -o -iname "*.fbx" -o -iname "*.dae" \) \
    | sort \
    | while read -r f; do
        size=$(du -h "$f" 2>/dev/null | cut -f1)
        rel="${f#"$ROOT/"}"
        echo "  $size  $rel"
    done

# ───────────────────────────────────────────────────────────────────────────────
# 2. Texture files on disk
# ───────────────────────────────────────────────────────────────────────────────
header "2. Texture files on disk (src/Resources/)"
find "$RESOURCES" -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" \
                            -o -iname "*.tga" -o -iname "*.bmp" \) \
    | sort \
    | while read -r f; do
        rel="${f#"$ROOT/"}"
        echo "  $rel"
    done

# ───────────────────────────────────────────────────────────────────────────────
# 3. scene.json models array — registered aliases
# ───────────────────────────────────────────────────────────────────────────────
header "3. scene.json registered model aliases"

python3 - "$SCENE_JSON" "$RESOURCES" <<'PYEOF'
import json, sys, os

scene_path = sys.argv[1]
resources  = sys.argv[2]

with open(scene_path) as f:
    scene = json.load(f)

# Build a flat map: lowercase_filename -> original_filename for all .obj files
disk_objs = {}
for dirpath, _, filenames in os.walk(resources):
    for fn in filenames:
        if fn.lower().endswith(".obj"):
            disk_objs[fn.lower()] = fn

models = scene.get("models", [])
if not models:
    print("  [WARN] No 'models' array found in scene.json")
    sys.exit(0)

for m in models:
    alias   = m.get("alias", "<no alias>")
    obj_ref = m.get("obj", "")
    tex     = m.get("texture", "")

    obj_status = "N/A"
    if obj_ref:
        exact_name  = obj_ref + ".obj"
        exact_lower = exact_name.lower()
        if exact_name in disk_objs.values():
            obj_status = "OK"
        elif exact_lower in disk_objs:
            found = disk_objs[exact_lower]
            obj_status = f"CASE_MISMATCH (found {found})"
        else:
            obj_status = f"MISSING ({exact_name} not found)"

    print(f"  alias={alias:<18} obj={obj_ref:<20} texture={tex:<20} file={obj_status}")
PYEOF

# ───────────────────────────────────────────────────────────────────────────────
# 4. Prefab JSON analysis
# ───────────────────────────────────────────────────────────────────────────────
header "4. Prefab JSON analysis"

python3 - "$PREFABS_DIR" "$RESOURCES" <<'PYEOF'
import json, sys, os

prefabs_dir = sys.argv[1]
resources   = sys.argv[2]

# Build a flat map: lowercase_filename -> original_filename for all .obj files
disk_objs = {}
for dirpath, _, filenames in os.walk(resources):
    for fn in filenames:
        if fn.lower().endswith(".obj"):
            disk_objs[fn.lower()] = fn

RESET  = "\033[0m" if sys.stdout.isatty() else ""
RED    = "\033[0;31m" if sys.stdout.isatty() else ""
GREEN  = "\033[0;32m" if sys.stdout.isatty() else ""
YELLOW = "\033[1;33m" if sys.stdout.isatty() else ""

issues = 0

for fname in sorted(os.listdir(prefabs_dir)):
    if not fname.endswith(".json"):
        continue
    path = os.path.join(prefabs_dir, fname)
    with open(path) as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError as e:
            print(f"  {RED}[PARSE ERROR]{RESET} {fname}: {e}")
            issues += 1
            continue

    alias       = data.get("alias") or data.get("id") or "<unknown>"
    model_blk   = data.get("model", {})
    obj_ref     = model_blk.get("obj", "")
    tex_ref     = model_blk.get("texture", "")
    mesh_ref    = data.get("mesh", "")
    render      = data.get("render_mode", "")
    has_physics = "physics" in data

    # Determine file status
    if not obj_ref and not mesh_ref:
        status = f"{RED}NO_MODEL{RESET}"
        issues += 1
    elif obj_ref:
        exact_name  = obj_ref + ".obj"
        exact_lower = exact_name.lower()
        if exact_name in disk_objs.values():
            status = f"{GREEN}OK{RESET}"
        elif exact_lower in disk_objs:
            found = disk_objs[exact_lower]
            status = f"{YELLOW}CASE_MISMATCH (disk={found}){RESET}"
            issues += 1
        else:
            status = f"{RED}MISSING ({exact_name} not on disk){RESET}"
            issues += 1
    else:
        # Animated mesh only — check if .glb is present anywhere under resources
        mesh_lower = mesh_ref.lower()
        resources_prefix = resources.rstrip(os.sep) + os.sep
        found_glb = False
        for dp, _, fns in os.walk(resources):
            for fn in fns:
                full = os.path.join(dp, fn)
                if full.startswith(resources_prefix):
                    rel = full[len(resources_prefix):].lower()
                    if rel == mesh_lower:
                        found_glb = True
                        break
            if found_glb:
                break
        if found_glb:
            status = f"{GREEN}OK (mesh){RESET}"
        else:
            status = f"{YELLOW}MESH_NOT_FOUND ({mesh_ref}){RESET}"

    obj_disp    = obj_ref  or "-"
    tex_disp    = tex_ref  or "-"
    mesh_disp   = mesh_ref or "-"
    render_disp = render   or "-"
    phys_disp   = "yes" if has_physics else f"{YELLOW}no{RESET}"

    print(f"  {fname:<30} alias={alias:<18} obj={obj_disp:<20} tex={tex_disp:<18}"
          f"  mesh={mesh_disp:<35} render={render_disp:<12} physics={phys_disp}  {status}")

print(f"\n  Prefab issues: {issues}")
PYEOF

# ───────────────────────────────────────────────────────────────────────────────
# 5. Cross-reference: aliases used in scene.json but not in models array
# ───────────────────────────────────────────────────────────────────────────────
header "5. scene.json alias cross-reference"

python3 - "$SCENE_JSON" <<'PYEOF'
import json, sys

with open(sys.argv[1]) as f:
    scene = json.load(f)

registered = {m["alias"] for m in scene.get("models", []) if "alias" in m}

used = set()
for section in ("entities", "editor_entities", "random"):
    for entry in scene.get(section, []):
        if "alias" in entry:
            used.add(entry["alias"])

RESET  = "\033[0m" if sys.stdout.isatty() else ""
RED    = "\033[0;31m" if sys.stdout.isatty() else ""
GREEN  = "\033[0;32m" if sys.stdout.isatty() else ""
YELLOW = "\033[1;33m" if sys.stdout.isatty() else ""

missing = used - registered
extra   = registered - used

if missing:
    for a in sorted(missing):
        print(f"  {RED}[UNREGISTERED]{RESET} alias '{a}' used in scene.json but not in models array")
else:
    print(f"  {GREEN}All aliases used in scene.json are registered in models array.{RESET}")

if extra:
    for a in sorted(extra):
        print(f"  [UNUSED]       alias '{a}' in models array but not used by any entity")
PYEOF

# ───────────────────────────────────────────────────────────────────────────────
# 6. Summary
# ───────────────────────────────────────────────────────────────────────────────
header "6. Summary"
if [ "$ISSUE_COUNT" -eq 0 ]; then
    echo -e "${GREEN}  No shell-level issues found. Check Python output above for model issues.${RESET}"
else
    echo -e "${RED}  $ISSUE_COUNT shell-level issue(s) detected. See above for details.${RESET}"
fi
