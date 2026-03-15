#!/usr/bin/env bash
# =============================================================================
# fix_lfs.sh — Migrate binary assets from regular git tracking to Git LFS.
#
# Problem:
#   Binary files (images, models, audio) committed directly to git inflate the
#   repository size and produce millions of diff lines.  They should be stored
#   in Git LFS instead, which keeps only a small pointer in the git object
#   database and uploads the actual bytes to the LFS server.
#
# What this script does:
#   1. Adds the common binary-asset extensions to .gitattributes so future
#      commits automatically use LFS.
#   2. Uses `git lfs migrate import` to rewrite the CURRENT BRANCH's history,
#      converting any matching files that are already tracked in git to LFS
#      pointers.  Only the commits reachable from HEAD are rewritten.
#   3. Prints a reminder to force-push and have collaborators re-clone.
#
# Usage:
#   chmod +x scripts/fix_lfs.sh
#   ./scripts/fix_lfs.sh [--dry-run]
#
#   --dry-run   Show which files WOULD be migrated without actually changing
#               anything.
#
# Requirements:
#   git >= 2.30, git-lfs >= 3.0
#
# IMPORTANT — read before running:
#   • This rewrites git history.  Anyone else with a clone must re-clone (or
#     run `git fetch && git reset --hard origin/main` on each branch) after
#     you force-push.
#   • The LFS server (GitHub, GitLab, etc.) must have LFS enabled for the repo.
#   • Run from the repository root.
#
# References:
#   https://git-lfs.github.com/
#   https://github.com/git-lfs/git-lfs/blob/main/docs/man/git-lfs-migrate.adoc
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
info()  { printf '\033[1;34m[INFO]\033[0m  %s\n' "$*"; }
warn()  { printf '\033[1;33m[WARN]\033[0m  %s\n' "$*"; }
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
    error "git-lfs not found.  Install it: https://git-lfs.github.com/"
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
    "*.jpg"
    "*.jpeg"
    "*.JPG"
    "*.JPEG"
    "*.png"
    "*.PNG"
    "*.bmp"
    "*.BMP"
    "*.tga"
    "*.TGA"
    "*.tiff"
    "*.TIFF"
    "*.hdr"
    "*.HDR"
    "*.exr"
    "*.EXR"
    "*.psd"
    "*.PSD"
    "*.mp3"
    "*.MP3"
    "*.wav"
    "*.WAV"
    "*.ogg"
    "*.OGG"
    "*.mp4"
    "*.MP4"
    "*.fbx"
    "*.FBX"
    "*.obj"
    "*.OBJ"
    "*.blend"
    "*.BLEND"
    "*.dae"
    "*.DAE"
    "*.zip"
    "*.ZIP"
    "*.tar.gz"
    "*.7z"
    "*.bin"
    "*.exe"
    "*.dll"
    "*.so"
    "*.dylib"
    "*.a"
    "*.lib"
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
            info "  [dry-run] Would add: $pat"
        else
            git lfs track "$pat" > /dev/null
            info "  Tracking: $pat"
            ADDED=$((ADDED + 1))
        fi
    else
        info "  Already tracked: $pat"
    fi
done

if [ "$DRY_RUN" -eq 0 ] && [ "$ADDED" -gt 0 ]; then
    git add "$GITATTRIBUTES"
    info ".gitattributes updated and staged."
fi

# ---------------------------------------------------------------------------
# Step 2: Find which tracked files would be migrated
# ---------------------------------------------------------------------------
info ""
info "Step 2: Scanning for binary files currently in git (not in LFS)..."

# Build a --include pattern list for git lfs migrate
INCLUDE_PATTERNS=""
for pat in "${LFS_EXTENSIONS[@]}"; do
    if [ -n "$INCLUDE_PATTERNS" ]; then
        INCLUDE_PATTERNS="$INCLUDE_PATTERNS,$pat"
    else
        INCLUDE_PATTERNS="$pat"
    fi
done

# Preview: what files would be migrated?
info ""
info "Files currently tracked by git that match the LFS patterns:"
git lfs migrate info \
    --include="$INCLUDE_PATTERNS" \
    --everything \
    2>/dev/null \
    | head -50 \
    || warn "Could not enumerate files (may need --everything flag on some git-lfs versions)"

if [ "$DRY_RUN" -eq 1 ]; then
    info ""
    warn "[dry-run] No changes made.  Remove --dry-run to execute the migration."
    exit 0
fi

# ---------------------------------------------------------------------------
# Step 3: Migrate history on the CURRENT branch only
# ---------------------------------------------------------------------------
info ""
info "Step 3: Migrating binary assets to LFS (rewriting current branch history)..."
warn "This rewrites commits.  Make sure you have a backup / are on a feature branch."
warn ""
read -r -p "Continue? [y/N] " REPLY
if [[ ! "$REPLY" =~ ^[Yy]$ ]]; then
    info "Aborted.  No changes were made."
    exit 0
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

git lfs migrate import \
    --include="$INCLUDE_PATTERNS" \
    --no-rewrite 2>/dev/null \
    || git lfs migrate import \
        --include="$INCLUDE_PATTERNS" \
        --include-ref="refs/heads/${CURRENT_BRANCH}"

info ""
info "LFS migration complete on branch: $CURRENT_BRANCH"

# ---------------------------------------------------------------------------
# Step 4: Verify
# ---------------------------------------------------------------------------
info ""
info "Step 4: Verification — files now stored in LFS:"
git lfs ls-files | tail -20

# ---------------------------------------------------------------------------
# Step 5: Instructions
# ---------------------------------------------------------------------------
info ""
info "============================================================"
info "  Next steps"
info "============================================================"
info ""
info "  1. Review the changes:"
info "       git status"
info "       git log --oneline -5"
info ""
info "  2. Force-push the rewritten branch:"
info "       git push --force-with-lease origin ${CURRENT_BRANCH}"
info ""
info "  3. Ask collaborators to re-clone (history was rewritten):"
info "       git clone <repo-url>"
info ""
info "  4. If the repo is on GitHub, make sure LFS is enabled:"
info "     https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage"
info ""
