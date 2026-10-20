#!/usr/bin/env python3
"""Regenerates blender/props/README.md from export/manifest.json."""
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(HERE, "export", "manifest.json")))
rows = []
for k, v in d.items():
    sx, sy, sz = v["size_xyz_m"]
    rows.append("| `%s` | %.2f x %.2f x %.2f | %.2f..%.2f | %d |" % (k, sx, sy, sz, v["min_xyz_m"][2], v["max_xyz_m"][2], v["tris"]))
head = open(os.path.join(HERE, "README.head.md")).read()
open(os.path.join(HERE, "README.md"), "w").write(head + "\n".join(rows) + "\n\nKeyboards include their cable (it leaves at +Y and lies on the table). Licence: code MIT, assets CC BY 4.0 (see LICENSE-ASSETS).\n")
