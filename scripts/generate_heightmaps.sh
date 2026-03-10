#!/usr/bin/env bash
# generate_heightmaps.sh
#
# Phase 4 Step 3 — Generate 27 heightmap tiles for streaming/chunking tests.
#
# This script copies the base heightMap.png into per-tile files named
# heightMap_X_Z.png for a 3×9 grid.
#
# Usage:
#   chmod +x scripts/generate_heightmaps.sh
#   ./scripts/generate_heightmaps.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEXTURE_DIR="$REPO_ROOT/src/Resources/Tutorial"
BASE_FILE="$TEXTURE_DIR/heightMap.png"

# Create the Tutorial texture directory if it doesn't already exist.
mkdir -p "$TEXTURE_DIR"

# If no base heightmap exists, create a small 16x16 grayscale PNG using Python.
if [ ! -f "$BASE_FILE" ]; then
    echo "[generate_heightmaps] No base heightMap.png found — generating a 16x16 test image."
    python3 << 'PYEOF'
import struct, zlib

width, height = 16, 16
raw = b''
for y in range(height):
    raw += b'\x00'
    for x in range(width):
        v = int(128 + 40 * ((x - 8)**2 + (y - 8)**2) / 128)
        v = max(0, min(255, v))
        raw += struct.pack('B', v)

def write_png(filename, w, h, raw_data):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0)
    compressed = zlib.compress(raw_data)
    with open(filename, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', ihdr))
        f.write(chunk(b'IDAT', compressed))
        f.write(chunk(b'IEND', b''))

import os
texture_dir = os.environ.get('TEXTURE_DIR', 'src/Resources/Tutorial')
write_png(os.path.join(texture_dir, 'heightMap.png'), width, height, raw)
print('[generate_heightmaps] Created base heightMap.png (16x16)')
PYEOF
fi

echo "[generate_heightmaps] Base heightmap: $BASE_FILE"

# Define the grid range for 27 tiles.
# X: -1 to 1 (3 values), Z: -4 to 4 (9 values) = 27 tiles
COUNT=0
for X in -1 0 1; do
    for Z in -4 -3 -2 -1 0 1 2 3 4; do
        DEST="$TEXTURE_DIR/heightMap_${X}_${Z}.png"
        if [ ! -f "$DEST" ]; then
            cp "$BASE_FILE" "$DEST"
            echo "  Created: heightMap_${X}_${Z}.png"
        else
            echo "  Exists:  heightMap_${X}_${Z}.png"
        fi
        COUNT=$((COUNT + 1))
    done
done

echo ""
echo "[generate_heightmaps] Done — $COUNT tile files ready in $TEXTURE_DIR"
echo "[generate_heightmaps] Grid: X=[-1..1], Z=[-4..4] (3x9 = 27 tiles)"
