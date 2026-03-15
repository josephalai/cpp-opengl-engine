#!/usr/bin/env bash
# =============================================================================
# fix_lfs.sh — Migrate binary assets from regular git tracking to Git LFS.
#            (WITHOUT rewriting history — only affects the NEXT commit)
#
# Problem:
# Binary files (images, models, audio) committed directly to git inflate the
# repository size and produce millions of diff lines. They should be stored
# in Git LFS instead, which keeps only a small pointer in the git object
# database and uploads the actual bytes to the LFS server.
#
# What this script does:
# 1. Adds the common binary-asset extensions to .gitattributes so future
#    commits automatically use LFS.
# 2. Finds binary files that are currently tracked by regular Git (matching
#    the patterns and not already in LFS).
# 3. Runs `git rm --cached` on those files (removes them from the index only)
#    then `git add` them back. Because .gitattributes now declares them as
#    LFS, Git stores only a pointer in the next commit. The actual files
#    stay on disk unchanged.
# 4. Prints a reminder to commit and push normally.
#
# Usage:
# chmod +x scripts/fix_lfs.sh
# ./scripts/fix_lfs.sh [--dry-run]
#
# --dry-run Show which files WOULD be migrated without actually changing
# anything.
#
# Requirements:
# git >= 2.30, git-lfs >= 3.0
#
# IMPORTANT — read before running:
# • This does NOT rewrite git history. Old commits keep the original binary
#   blobs. Only the commit you make after running this script will use LFS.
# • No force-push is needed. Collaborators simply run `git pull`.
# • Repository size on disk will not shrink until you rewrite history later
#   (if you ever want to). LFS objects are stored on the LFS server.
# • The LFS server (GitHub, GitLab, etc.) must have LFS enabled for the repo.
# • Run from the repository root.
#
# References:
# https://git-lfs.github.com/
# https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-track.adoc
# =============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
info() { printf '\033[1;34m[INFO]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[WARN]\033[0m %s\n' "$*"; }
error() { printf '\033[1;31m[ERROR]\033[0m %s\n' "$*" >&2; }

DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        *) error "Unknown argument: $arg"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Verify prerequisites
# ---------------------------------------------------------------------------
if ! command -v git >/dev/null 2>&1; then
    error "git not found in PATH."
    exit 1
fi
if ! command -v git-lfs >/dev/null 2>&1 && ! git lfs version >/dev/null 2>&1; then
    error "git-lfs not found. Install it: https://git-lfs.github.com/"
    exit 1
fi
ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    error "Not inside a git repository."
    exit 1
}
cd "$ROOT"

# ---------------------------------------------------------------------------
# Extensions to migrate to LFS
# ---------------------------------------------------------------------------
# Add or extend this list as needed.
LFS_EXTENSIONS=(
    "*.jpg" "*.jpeg" "*.JPG" "*.JPEG"
    "*.png" "*.PNG"
    "*.bmp" "*.BMP"
    "*.tga" "*.TGA"
    "*.tiff" "*.TIFF"
    "*.hdr" "*.HDR"
    "*.exr" "*.EXR"
    "*.psd" "*.PSD"
    "*.mp3" "*.MP3"
    "*.wav" "*.WAV"
    "*.ogg" "*.OGG"
    "*.mp4" "*.MP4"
    "*.fbx" "*.FBX"
    "*.obj" "*.OBJ"
    "*.blend" "*.BLEND"
    "*.dae" "*.DAE"
    "*.zip" "*.ZIP"
    "*.tar.gz"
    "*.7z"
    "*.bin"
    "*.exe" "*.dll" "*.so" "*.dylib" "*.a" "*.lib"
)

# Already tracked by LFS (from .gitattributes)?
already_lfs() {
    local ext="$1"
    git lfs track 2>/dev/null | grep -qF "$ext"
}

# ---------------------------------------------------------------------------
# Step 1: Update .gitattributes
# ---------------------------------------------------------------------------
info "Step 1: Updating .gitattributes for LFS tracking..."
GITATTRIBUTES="$ROOT/.gitattributes"
ADDED=0
for pat in "${LFS_EXTENSIONS[@]}"; do
    if ! already_lfs "$pat"; then
        if [ "$DRY_RUN" -eq 1 ]; then
            info " [dry-run] Would add: $pat"
        else
            git lfs track "$pat" > /dev/null
            info " Tracking: $pat"
            ADDED=$((ADDED + 1))
        fi
    else
        info " Already tracked: $pat"
    fi
done
if [ "$DRY_RUN" -eq 0 ] && [ "$ADDED" -gt 0 ]; then
    git add "$GITATTRIBUTES"
    info ".gitattributes updated and staged."
fi

# ---------------------------------------------------------------------------
# Step 2: Find which tracked files would be migrated (current index only)
# ---------------------------------------------------------------------------
info ""
info "Step 2: Scanning for binary files currently tracked by git (not in LFS)..."

# One-time list of files already in LFS
mapfile -t LFS_TRACKED < <(git lfs ls-files --name-only 2>/dev/null || true)
declare -A lfs_set
for f in "${LFS_TRACKED[@]}"; do
    lfs_set["$f"]=1
done

# Collect files to migrate (tracked by git, matches pattern, not in LFS)
declare -A seen
MIGRATE_FILES=()
for pat in "${LFS_EXTENSIONS[@]}"; do
    mapfile -t FILES < <(git ls-files "$pat" 2>/dev/null || true)
    for f in "${FILES[@]}"; do
        if [ -n "$f" ] && [ -z "${lfs_set[$f]+set}" ] && [ -z "${seen[$f]+set}" ]; then
            MIGRATE_FILES+=("$f")
            seen["$f"]=1
        fi
    done
done

if [ ${#MIGRATE_FILES[@]} -eq 0 ]; then
    info "No binary files need migration (none tracked or all already in LFS)."
else
    info "Files that will be converted to LFS in the NEXT commit:"
    printf '  - %s\n' "${MIGRATE_FILES[@]}" | head -50
    if [ ${#MIGRATE_FILES[@]} -gt 50 ]; then
        info "  ... and ${#MIGRATE_FILES[@]} total"
    fi
fi

if [ "$DRY_RUN" -eq 1 ]; then
    info ""
    if [ ${#MIGRATE_FILES[@]} -gt 0 ]; then
        warn "[dry-run] Would run: git rm --cached + git add on the files above"
    fi
    warn "[dry-run] No changes made. Remove --dry-run to execute the migration."
    exit 0
fi

# ---------------------------------------------------------------------------
# Step 3: Convert current files to LFS (next commit only)
# ---------------------------------------------------------------------------
info ""
info "Step 3: Converting binary files to LFS (next commit only)..."
warn "This does NOT rewrite history — only the next commit you make is affected."
warn ""
read -r -p "Continue? [y/N] " REPLY
if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
    info "Aborted. No changes were made."
    exit 0
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [ ${#MIGRATE_FILES[@]} -gt 0 ]; then
    info "Untracking ${#MIGRATE_FILES[@]} files from regular Git..."
    printf '%s\0' "${MIGRATE_FILES[@]}" | xargs -0 git rm --cached --
    info "Re-adding ${#MIGRATE_FILES[@]} files as LFS pointers..."
    printf '%s\0' "${MIGRATE_FILES[@]}" | xargs -0 git add --
else
    info "No files to convert."
fi

info ""
info "LFS conversion complete (changes staged) on branch: $CURRENT_BRANCH"

# ---------------------------------------------------------------------------
# Step 4: Verify
# ---------------------------------------------------------------------------
info ""
info "Step 4: Verification — files now staged as LFS:"
git lfs ls-files | tail -20
info ""
info "Staged changes (review before committing):"
git status

# ---------------------------------------------------------------------------
# Step 5: Instructions
# ---------------------------------------------------------------------------
info ""
info "============================================================"
info " Next steps"
info "============================================================"
info ""
info " 1. Commit the migration:"
info "    git commit -m \"Migrate binary assets to Git LFS\""
info ""
info " 2. Push normally (no --force needed):"
info "    git push origin ${CURRENT_BRANCH}"
info ""
info " 3. Ask collaborators to update:"
info "    git pull"
info "    (Git LFS will automatically download the objects.)"
info "    If any files are missing after pull: git lfs pull"
info ""
info " 4. If the repo is on GitHub, make sure LFS is enabled:"
info "    https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage"
info ""
info "Note: Old commits still contain the full binaries. Repo size on disk"
info "      will not shrink until you rewrite history later (optional)."
info ""
