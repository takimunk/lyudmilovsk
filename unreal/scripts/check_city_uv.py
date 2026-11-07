"""Commandlet check: SM_<type>_Windows keep UV1 (the light texel) through the Interchange import.
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/check_city_uv.py [types]"
Logs "CITYUV <mesh>: N uv channels, uv1 sample ..." and whether the UV1 values sit on texel centres (k+0.5)/size."""
import sys

import unreal

eal = unreal.EditorAssetLibrary
types = sys.argv[1].split(",") if len(sys.argv) > 1 else None
for path in eal.list_assets("/Game/City/Buildings", True):
    if not path.split(".")[0].endswith("_Windows"):
        continue
    if types and not any("/%s/" % t in path for t in types):
        continue
    sm = eal.load_asset(path)
    if not isinstance(sm, unreal.StaticMesh):
        continue
    n = sm.get_num_uv_channels(0)
    md = sm.get_static_mesh_description(0)
    vals = []
    if n > 1:
        cnt = md.get_vertex_instance_count()
        for i in range(0, cnt, max(1, cnt // 8)):
            uv = md.get_vertex_instance_uv(unreal.VertexInstanceID(i), 1)
            vals.append((round(uv.x, 5), round(uv.y, 5)))
    # texel centres: u*size - 0.5 is an integer for some power-of-two size
    def centred(v, size):
        return abs(v * size - 0.5 - round(v * size - 0.5)) < 1e-3
    sizes = [s for s in (16, 32, 64, 128, 256) if vals and all(centred(u, s) and centred(v, s) for u, v in vals)]
    unreal.log_warning("CITYUV %s: %d uv channels, uv1 sample %s, texel centres for size %s" % (
        sm.get_name(), n, vals[:8], sizes or "none"))
