"""Topi, the dune land east of the town (docs/topi.md): what the Unreal side needs from the map.

  python3 tools/topi.py            -> unreal/Lyudmilovsk/Content/Topi/topi_edge.tbl

The sand of Topi lies over the last ~160 m of the map's landscape along its whole east (inland) edge
(X -1140..-1300) and goes on past it on its own ground (Source/.../Private/Topi/TopiModel.cpp). Where the two overlap
the model needs the map's own height (W.height) to bring the sand up from under it: this writes that strip as a
table. Rerun whenever tools/world_map.py changes the land east of X -1100 or the map's Y extent (--check fails if
the committed table is stale).
"""
import os
import struct
import sys

import world_map as W

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Topi", "topi_edge.tbl")
CELL = 6.0
X0, X1 = W.X_MIN - 66.0, W.X_MIN + 174.0
Y0, Y1 = W.Y_MIN - 60.0, W.Y_MAX + 60.0


def table():
    nx = int(round((X1 - X0) / CELL)) + 1
    ny = int(round((Y1 - Y0) / CELL)) + 1
    z = []
    for j in range(ny):
        y = min(max(Y0 + j * CELL, W.Y_MIN), W.Y_MAX)       # past the map's edge: its last row
        for i in range(nx):
            x = min(max(X0 + i * CELL, W.X_MIN), W.X_MAX)
            z.append(W.height(x, y))
    return struct.pack("<4siiifff", b"TOPE", 2, nx, ny, X0, Y0, CELL) + struct.pack("<%df" % len(z), *z)


if __name__ == "__main__":
    data = table()
    if "--check" in sys.argv:
        old = open(OUT, "rb").read() if os.path.exists(OUT) else b""
        if old != data:
            sys.exit("topi_edge.tbl is stale: rerun python3 tools/topi.py")
        print("topi_edge.tbl is up to date")
    else:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        open(OUT, "wb").write(data)
        print("wrote", OUT, len(data), "bytes")
