"""The woods' cover mask for the game: world_map.forest_cover(x, y) sampled every STEP metres over the whole map and
written as Content/World/forest_cover.fcv, which AProceduralForest reads to drop the trees the mask leaves out (so a
wood has a natural outline whatever rectangles its tiles are). Pure Python. docs/north.md "Woods".

The file: bytes "FCV1", int32 nx, ny, float32 x0, y0, step (map metres), then nx * ny uint8 (255 = every tree stands),
rows from y0 north, each from x0 toward +X. unreal/scripts/import_world.py writes it before it lays the forests.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import world_map as W  # noqa: E402

STEP = 8.0
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "unreal", "Lyudmilovsk", "Content",
                                    "World", "forest_cover.fcv"))


def grid():
    """(nx, ny, x0, y0, bytes)."""
    nx = int((W.X_MAX - W.X_MIN) / STEP) + 1
    ny = int((W.Y_MAX - W.Y_MIN) / STEP) + 1
    v = bytearray()
    for j in range(ny):
        y = W.Y_MIN + j * STEP
        for i in range(nx):
            v.append(int(round(255.0 * min(1.0, max(0.0, W.forest_cover(W.X_MIN + i * STEP, y))))))
    return nx, ny, W.X_MIN, W.Y_MIN, bytes(v)


def write(path=OUT):
    nx, ny, x0, y0, v = grid()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"FCV1" + struct.pack("<iifff", nx, ny, x0, y0, STEP) + v)
    return path, nx, ny, 100.0 * sum(v) / (255.0 * len(v))


if __name__ == "__main__":
    print("forest cover: %s, %d x %d, %.0f%% kept" % write(*sys.argv[1:2]))
