#!/usr/bin/env python3
"""The cabin's self-check without Unreal (tools/spacecraft/CABIN_NOTES.md).

Reads Private/Spacecraft/LyudSpacecraftCabin.cpp and verifies every name it uses against the things that own it:

  kit.json paths      K.Num / K.Vec / K.Frame / K.Arr("a.b"), FKit::ENum / EVec / EFrame / EStr / EArr(E, "list[]", "rel"),
                      InteractAt("id")                          -> Content/Spacecraft/kit.json (and records.json)
  meshes              LoadMesh("SM_SC_x")                       -> the glb's nodes (its JSON chunk) and kit.json "pieces"
  slots, materials    MakeMid(O, Mid, comp, "slot", "material") -> the piece's slots, the glb's materials, and the
                                                                   material import_spacecraft.py puts in that slot
  parameters          FParamNames (// PARAM materials), and every Scalar / Vector / Texture(O, Mid, P().Name)
                                                                -> the parameters import_spacecraft.py's MATERIALS declare
  localized textures  LyudText::LoadTexture("Spacecraft", "T") -> textures/spacecraft/T_<lang>.png for ru, en, ja
  the model           the lamp board's order (kit) against FlightModel.h's LampDef
  the header          every method ALyudSpacecraft declares is defined; ENGINE_NOTES' action ids all have a point
  the kit's frames    {at, x, y, z, rot}: rot (FRotator) gives the same axes
  the picking         ActionAt's rule, run here from the seat and from the standing point at every target

Exit 1 on any name that does not exist.    python3 tools/spacecraftsim/cabincheck.py [-v]
"""
import importlib.util
import json
import math
import os
import re
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Source", "Lyudmilovsk")
CPP = os.environ.get("CABINCHECK_CPP") or os.path.join(SRC, "Private", "Spacecraft", "LyudSpacecraftCabin.cpp")   # (the variable: for testing this check on a broken copy)
HEADER = os.path.join(SRC, "Public", "Spacecraft", "LyudSpacecraft.h")
MODEL = os.path.join(SRC, "Private", "Spacecraft", "FlightModel.h")
KIT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "kit.json")
RECORDS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "records.json")
GLB = os.path.join(ROOT, "design", "spacecraft", "game", "spacecraft_kit.glb")
IMPORTER = os.path.join(ROOT, "unreal", "scripts", "import_spacecraft.py")
TEXTURES = os.path.join(ROOT, "textures", "spacecraft")
VERBOSE = "-v" in sys.argv

bad, notes = [], []
counts = {}


def fail(msg):
    bad.append(msg)


def count(what, n=1):
    counts[what] = counts.get(what, 0) + n


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


# ---------------------------------------------------------------------------------------------------------- inputs
cpp = open(CPP, encoding="utf-8").read()
kit = json.load(open(KIT, encoding="utf-8"))
records = json.load(open(RECORDS, encoding="utf-8")) if os.path.isfile(RECORDS) else None

glb_nodes, glb_materials = set(), set()
if os.path.isfile(GLB):
    with open(GLB, "rb") as f:
        magic, _version, _length = struct.unpack("<III", f.read(12))
        chunk_len, chunk_type = struct.unpack("<II", f.read(8))
        if magic != 0x46546C67 or chunk_type != 0x4E4F534A:
            fail("%s is not a glb with a JSON chunk first" % GLB)
        else:
            g = json.loads(f.read(chunk_len).decode("utf-8"))
            glb_nodes = {n.get("name") for n in g.get("nodes", [])}
            glb_materials = {m.get("name") for m in g.get("materials", [])}
else:
    notes.append("no %s here: mesh and slot names are checked against kit.json only" % os.path.relpath(GLB, ROOT))

spec = importlib.util.spec_from_file_location("import_spacecraft_for_cabincheck", IMPORTER)
imp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(imp)


def material_params(node, out):
    """Every ("scalar"|"vector"|"tex"|"sample", name, ...) inside a material's spec."""
    if isinstance(node, tuple) and len(node) >= 2 and node[0] in ("scalar", "vector", "tex", "sample") and isinstance(node[1], str):
        out[node[1]] = {"scalar": "scalar", "vector": "vector", "tex": "texture", "sample": "texture"}[node[0]]
    if isinstance(node, (tuple, list)):
        for x in node:
            material_params(x, out)
    elif isinstance(node, dict):
        for x in node.values():
            material_params(x, out)


PARAMS = {}
for name, mat_spec in imp.MATERIALS.items():
    PARAMS[name] = {}
    material_params(mat_spec, PARAMS[name])
# the instances the importer makes (build_materials): their parameters are their parent's
PARENT = {"MI_SC_GlassCover": "M_SC_Glass", "MI_SC_Labels_ru": "M_SC_Labels", "MI_SC_Labels_en": "M_SC_Labels", "MI_SC_Labels_ja": "M_SC_Labels"}
importer_text = open(IMPORTER, encoding="utf-8").read()
for inst, parent in PARENT.items():
    if not re.search(r'instance\(\s*"%s"\s*,\s*mats\["%s"\]' % (inst, parent), importer_text) and not re.search(r'"MI_SC_Labels_" \+ lang, mats\["M_SC_Labels"\]', importer_text):
        fail("import_spacecraft.py no longer makes %s from %s" % (inst, parent))


def base_material(name):
    return PARENT.get(name, name)


# ---------------------------------------------------------------------------------------------------------- kit paths
def walk(node, path):
    """All values at a dotted path; 'a[]' goes through every element of the list a."""
    nodes = [node]
    for key in [k for k in path.split(".") if k]:
        each = key.endswith("[]")
        key = key[:-2] if each else key
        nxt = []
        for n in nodes:
            if isinstance(n, dict) and key in n:
                v = n[key]
                if each:
                    if isinstance(v, list):
                        nxt.extend(v)
                else:
                    nxt.append(v)
        nodes = nxt
    return nodes


KINDS = {"Num": (int, float), "Vec": list, "Frame": dict, "Arr": list, "Str": str}


def right_kind(kind, v):
    if kind == "Frame":
        return isinstance(v, dict) and all(k in v for k in ("at", "rot"))
    if kind == "Vec":
        return isinstance(v, list) and len(v) >= 3 and all(isinstance(x, (int, float)) for x in v[:3])
    if kind == "Num":
        return isinstance(v, (int, float)) and not isinstance(v, bool)
    return isinstance(v, KINDS[kind])


for m in re.finditer(r'\bK\.(Num|Vec|Frame|Arr)\(\s*TEXT\("([^"]+)"\)', cpp):
    kind, path = m.group(1), m.group(2)
    got = walk(kit, path)
    count("kit paths")
    if not got:
        fail("%d: kit.json has no %s" % (line_of(cpp, m.start()), path))
    elif not right_kind(kind, got[0]):
        fail("%d: kit.json %s is not a %s" % (line_of(cpp, m.start()), path, kind))

for m in re.finditer(r'\bFKit::E(Num|Vec|Frame|Str|Arr)\(\s*[^,]+,\s*TEXT\("([^"]+)"\)\s*,\s*TEXT\("([^"]*)"\)', cpp):
    kind, base, rel = m.group(1), m.group(2), m.group(3)
    count("kit list fields")
    if base == "records.json":
        elems = (records or {}).get("records", [])
        if records is None:
            notes.append("no Content/Spacecraft/records.json: the rack shows cells 0..7 in order")
            continue
    else:
        elems = walk(kit, base)
    if not elems:
        fail("%d: kit.json has no list %s" % (line_of(cpp, m.start()), base))
        continue
    have = [e for e in elems if walk(e, rel)]
    if not have:
        fail("%d: no element of %s has %s" % (line_of(cpp, m.start()), base, rel))
    else:
        if not right_kind(kind, walk(have[0], rel)[0]):
            fail("%d: %s . %s is not a %s" % (line_of(cpp, m.start()), base, rel, kind))
        if len(have) != len(elems):
            notes.append("%s . %s is in %d of %d elements (the code's default covers the rest)" % (base, rel, len(have), len(elems)))

interact_ids = {e.get("id") for e in kit.get("interact", [])}
for m in re.finditer(r'\bInteractAt\(\s*TEXT\("([^"]+)"\)', cpp):
    count("interact ids")
    if m.group(1) not in interact_ids:
        fail("%d: kit.json interact has no id %s" % (line_of(cpp, m.start()), m.group(1)))

# FKit::ToFrame / ToVec straight on a list's elements: the list itself went through K.Arr above

# ---------------------------------------------------------------------------------------------------------- meshes
pieces = kit.get("pieces", {})
for m in re.finditer(r'\bLoadMesh\(\s*TEXT\("([^"]+)"\)', cpp):
    count("meshes")
    name = m.group(1)
    if name not in pieces:
        fail("%d: kit.json pieces has no %s" % (line_of(cpp, m.start()), name))
    if glb_nodes and name not in glb_nodes:
        fail("%d: the glb has no object %s" % (line_of(cpp, m.start()), name))
if "/Game/Spacecraft/Meshes/" not in cpp or imp.MESHES != "/Game/Spacecraft/Meshes":
    fail("the meshes' folder is not import_spacecraft.py's %s" % imp.MESHES)
loaded = set(re.findall(r'\bLoadMesh\(\s*TEXT\("([^"]+)"\)', cpp))
for name in sorted(set(pieces) - loaded - {"SM_SC_Stage"}):
    fail("the kit's piece %s is never loaded by the cabin" % name)

# ---------------------------------------------------------------------------------------------------------- slots -> materials
# which mesh a component variable holds, read in the order of the file
mesh_var, comp_mesh, mid_material = {}, {}, {}
token = re.compile(
    r'UStaticMesh\*\s+(?P<mv>\w+)\s*=\s*LoadMesh\(\s*TEXT\("(?P<mvm>\w+)"\)\)'
    r'|(?:UStaticMeshComponent\*\s+)?(?P<cv>\w+(?:\[[^\]]+\])?)\s*=\s*AddMesh\(\s*O\s*,\s*TEXT\("\w+"\)\s*,\s*(?:LoadMesh\(\s*TEXT\("(?P<cm>\w+)"\)\)|(?P<cmv>\w+))\s*,'
    r'|MakeMid\(\s*O\s*,\s*(?P<mid>\w+)[^,]*,\s*(?P<mc>\w+)\s*,\s*TEXT\("(?P<slot>[^"]+)"\)\s*,\s*(?:TEXT\("(?P<mat>[^"]+)"\)|(?P<null>nullptr))\s*\)')
for m in token.finditer(cpp):
    ln = line_of(cpp, m.start())
    if m.group("mv"):
        mesh_var[m.group("mv")] = m.group("mvm")
    elif m.group("cv"):
        mesh = m.group("cm") or mesh_var.get(m.group("cmv"))
        if mesh is None:
            fail("%d: cabincheck cannot tell which mesh %s is" % (ln, m.group("cmv")))
        comp_mesh[m.group("cv")] = mesh
    elif m.group("mid"):
        count("dynamic materials")
        mid, comp, slot, mat = m.group("mid"), m.group("mc"), m.group("slot"), m.group("mat")
        mesh = comp_mesh.get(comp)
        if mesh is None:
            fail("%d: cabincheck cannot tell which mesh component %s is" % (ln, comp))
            continue
        if slot not in pieces.get(mesh, {}).get("slots", []):
            fail("%d: %s has no slot %s (kit.json pieces)" % (ln, mesh, slot))
        if glb_materials and slot not in glb_materials:
            fail("%d: the glb has no material %s" % (ln, slot))
        want = imp.slot_name(slot, mesh)
        if mat is None:
            if want is not None or slot.lower() not in imp.FLAT_SLOTS:
                fail("%d: slot %s of %s is not one of the glb's flat materials (the importer gives it %s)" % (ln, slot, mesh, want))
            mat = "glb"
        elif want != mat:
            fail("%d: import_spacecraft.py puts %s in slot %s of %s, the code expects %s" % (ln, want, slot, mesh, mat))
        if mid_material.setdefault(mid, mat) != mat:
            fail("%d: %s is made for %s and for %s" % (ln, mid, mid_material[mid], mat))
        if mat != "glb" and base_material(mat) not in PARAMS:
            fail("%d: import_spacecraft.py builds no material %s" % (ln, mat))
plain = re.sub(r'/\*.*?\*/', '', re.sub(r'//[^\n]*', '', cpp), flags=re.S)
if plain.count("MakeMid(") - 1 != counts.get("dynamic materials", 0):
    fail("cabincheck read %d of the %d MakeMid calls: keep them on the pattern MakeMid(O, Mid, comp, TEXT(slot), TEXT(material))" % (counts.get("dynamic materials", 0), plain.count("MakeMid(") - 1))

# ---------------------------------------------------------------------------------------------------------- parameters
param_names = {}
for m in re.finditer(r'const FName (\w+) = TEXT\("(\w+)"\);\s*//\s*PARAM\s+([^\n]+)', cpp):
    ident, name, mats = m.group(1), m.group(2), m.group(3).split()
    param_names[ident] = name
    count("parameters")
    for mat in mats:
        if mat == "glb":
            continue
        if mat not in PARAMS:
            fail("%d: no material %s in import_spacecraft.py" % (line_of(cpp, m.start()), mat))
        elif name not in PARAMS[mat]:
            fail("%d: %s has no parameter %s" % (line_of(cpp, m.start()), mat, name))
KIND_OF = {"Scalar": "scalar", "Vector": "vector", "Texture": "texture"}
used = set()
for m in re.finditer(r'\b(?:FCabinData::)?(Scalar|Vector|Texture)\(\s*(?:O|\*this)\s*,\s*(?:FCabinData::)?(Mid\w+)[^,]*,\s*(?:FCabinData::)?P\(\)\.(\w+)', cpp):
    kind, mid, ident = m.group(1), m.group(2), m.group(3)
    ln = line_of(cpp, m.start())
    count("parameter sets")
    used.add(ident)
    if ident not in param_names:
        fail("%d: P().%s is not in FParamNames" % (ln, ident))
        continue
    mat = mid_material.get(mid)
    if mat is None:
        fail("%d: %s has no MakeMid" % (ln, mid))
    elif mat != "glb":
        have = PARAMS.get(base_material(mat), {})
        if param_names[ident] not in have:
            fail("%d: %s (%s) has no parameter %s" % (ln, mat, mid, param_names[ident]))
        elif have[param_names[ident]] != KIND_OF[kind]:
            fail("%d: %s of %s is a %s parameter, set as a %s" % (ln, param_names[ident], mat, have[param_names[ident]], KIND_OF[kind]))
for ident in sorted(set(param_names) - used):
    fail("FParamNames::%s is never set" % ident)
# no parameter is set past the helpers, and none by a bare string
raw_sets = len(re.findall(r'->Set(?:Scalar|Vector|Texture)ParameterValue\(', cpp))
if raw_sets != 3:
    fail("%d direct Set*ParameterValue calls (the three helpers Scalar / Vector / Texture are the only ones allowed)" % raw_sets)
if re.search(r'ParameterValue\(\s*TEXT\(', cpp):
    fail("a material parameter is named by a bare string: put it in FParamNames")
all_sets = len(re.findall(r'(?<![\w>])(?:FCabinData::)?(?:Scalar|Vector|Texture)\(\s*(?:O|\*this)\s*,', plain))
if all_sets != counts.get("parameter sets", 0):
    fail("cabincheck read %d of the %d Scalar / Vector / Texture calls: keep them on the pattern X(O, MidY, P().Z, ...)" % (counts.get("parameter sets", 0), all_sets))

# ---------------------------------------------------------------------------------------------------------- textures
for m in re.finditer(r'LyudText::LoadTexture\(\s*TEXT\("(\w+)"\)\s*,\s*TEXT\("(\w+)"\)', cpp):
    count("localized textures")
    folder, name = m.group(1), m.group(2)
    if "/Game/" + folder != imp.ROOT:
        fail("%d: LoadTexture folder %s is not the importer's %s" % (line_of(cpp, m.start()), folder, imp.ROOT))
    for lang in imp.LANGS:
        if not os.path.isfile(os.path.join(TEXTURES, "%s_%s.png" % (name, lang))):
            fail("%d: no textures/spacecraft/%s_%s.png" % (line_of(cpp, m.start()), name, lang))

# ---------------------------------------------------------------------------------------------------------- the model, the header
model = open(MODEL, encoding="utf-8").read()
defs = re.search(r'static const FLampDef Defs\[LampCount\] = \{(.*?)\};', model, re.S)
lamp_ids = re.findall(r'\{\s*"(\w+)"\s*,\s*"(\w+)"', defs.group(1)) if defs else []
cells = kit["panel"]["lamps"]["cells"]
if len(lamp_ids) != 24 or len(cells) != 24:
    fail("the lamp board: %d lamps in FlightModel.h, %d cells in kit.json" % (len(lamp_ids), len(cells)))
for i, (lamp, cell) in enumerate(zip(lamp_ids, cells)):
    count("lamps")
    if lamp[0] != cell["id"] or cell["index"] != i:
        fail("lamp %d: the model's %s, the kit's %s (index %s)" % (i, lamp[0], cell["id"], cell["index"]))
    if lamp[1][0] not in "garw":
        fail("lamp %d: colour %s has no class in the cabin" % (i, lamp[1]))
for field in ("bPowered", "bLampTest", "ClockDigits", "RangeKm", "SpeedShown", "bSpeedMs", "AltDialKm", "GlobeLatDeg", "GlobeLonRelDeg", "CrtEuropaX",
              "CrtPlanetR", "CrtProgress", "CrtPhase", "CrtMode", "bCabinLight", "bRadio", "Hull01", "GMeter", "PressMmHg", "O2Pct", "Co2Pct", "TempC",
              "Volts", "FuelPct", "RollDeg", "PitchDeg", "YawDeg", "GLoad", "Shake", "bRealWorld", "bFlying", "Switches", "Lamps", "SwGyro", "SwitchCount",
              "LampCount", "IslandLatDeg = 18.0"):
    if not re.search(r'\b%s\b' % re.escape(field), model):
        fail("FlightModel.h has no %s" % field)
for field in re.findall(r'\b(?:In|S)\.(\w+)', cpp[cpp.find("::Instruments(ALyudSpacecraft& O, const LyudFlight::FState& S"):cpp.find("// ------------------------------------------------------------------ ALyudSpacecraft")]):
    count("model fields")
    if not re.search(r'\b%s\b' % field, model):
        fail("the model has no field %s" % field)
if "MapLat = 54.96, MapLon = 20.40" not in cpp or tuple(imp.EU_ISLAND[:2]) != (54.96, 20.40):
    fail("the globe's island (ModelToMap) is not the map's own (import_spacecraft.py EU_ISLAND %s)" % (imp.EU_ISLAND,))

header = open(HEADER, encoding="utf-8").read()
cls = header[header.find("class LYUDMILOVSK_API ALyudSpacecraft "):header.find("class LYUDMILOVSK_API ALyudSpacecraftStage")]
for m in re.finditer(r'^\s*(?:virtual\s+)?[\w:<>\*& ]*?\b(\w+)\(([^;{]*)\)\s*(?:const)?\s*(?:override)?\s*;', cls, re.M):
    if m.group(1).isupper() or m.group(1) == "GENERATED_BODY":
        continue
    count("methods")
    if not re.search(r'\bALyudSpacecraft::%s\(' % m.group(1), cpp):
        fail("the header declares ALyudSpacecraft::%s, the cabin does not define it" % m.group(1))

points_body = cpp[cpp.find("void ALyudSpacecraft::FCabinData::BuildPoints"):cpp.find("bool ALyudSpacecraft::FCabinData::PointNow")]
point_ids = set(re.findall(r'TEXT\("([a-z_0-9]+)"\)', points_body))
WANTED = "hatch wheel sit buckle sw_power sw_air sw_gyro sw_seal sw_arm cover launch sw_light sw_crt sw_radio port_big port_left port_right " \
         "player needle thermos book nap chute_cover chute_handle speaker panel couch fan_0 fan_1 hull_0 hull_1 hull_2 hull_3 jets".split()
for i in WANTED:
    count("action ids")
    if i not in point_ids:
        fail("no interaction point for the director's id %s" % i)
anim_ids = set(re.findall(r'= TEXT\("([a-z_]+)"\)', cpp[cpp.find("struct FIdNames"):cpp.find("static const FIdNames& Ids()")]))
for i in "hatch wheel cover launch chute_cover chute_handle buckle book thermos sw_light sw_crt sw_radio big left right".split():
    if i not in anim_ids:
        fail("PlayActionAnim / PortholeView do not know %s" % i)


# ---------------------------------------------------------------------------------------------------------- the kit's frames
def rotator_axes(pitch, yaw, roll):
    """FRotationMatrix's axes (Unreal's own formula)."""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return ((cp * cy, cp * sy, sp), (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp), (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))


def frames(node, path):
    if isinstance(node, dict):
        if all(k in node for k in ("at", "x", "y", "z", "rot")):
            yield path, node
        for k, v in node.items():
            yield from frames(v, path + "." + k if path else k)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from frames(v, "%s[%d]" % (path, i))


for path, fr in frames(kit, ""):
    count("frames")
    axes = rotator_axes(*fr["rot"])
    worst = max(abs(a - b) for axis, key in zip(axes, "xyz") for a, b in zip(axis, fr[key]))
    if worst > 0.004:
        fail("kit.json %s: rot %s does not give its axes (off by %.4f)" % (path, fr["rot"], worst))

# ---------------------------------------------------------------------------------------------------------- the file itself
code = re.sub(r'//[^\n]*', '', cpp)
code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
code = re.sub(r"'(?:\\.|[^'\\])'", "''", code)
for a, b in ("{}", "()", "[]"):
    if code.count(a) != code.count(b):
        fail("unbalanced %s%s in the .cpp: %d against %d" % (a, b, code.count(a), code.count(b)))
depth = 0
for i, ch in enumerate(code):
    depth += ch == "{"
    depth -= ch == "}"
    if depth < 0:
        fail("a } too many near line %d" % line_of(code, i))
        break
if re.search(r'\busing namespace LyudFlight\b', cpp):
    fail("using namespace LyudFlight (it has its own FQuat)")
for inc in re.findall(r'#include "([^"]+)"', cpp):
    count("includes")
    here = [os.path.join(SRC, "Public", inc), os.path.join(SRC, "Private", inc)]
    engine = inc.split("/")[0] in ("Components", "Dom", "Engine", "HAL", "Materials", "Misc", "Serialization")
    if not engine and not any(os.path.isfile(p) for p in here):
        fail("#include \"%s\": no such file in the module" % inc)


# ---------------------------------------------------------------------------------------------------------- the picking
def vec(v):
    return tuple(float(x) for x in v[:3])


def add(a, b, s=1.0):
    return tuple(x + y * s for x, y in zip(a, b))


def frame_point(fr, p):
    out = vec(fr["at"])
    for axis, c in zip("xyz", p):
        out = add(out, vec(fr[axis]), c)
    return out


consts = dict((k, float(v)) for k, v in re.findall(r'(\w+) = ([0-9.]+)f[,;]', points_body))
interact = {e["id"]: vec(e["at"]) for e in kit["interact"]}
door = kit["hatch"]["closed"]
targets = []       # (id, at, radius, reach, needs)
for m in re.finditer(r'Add\((?:TEXT\("(\w+)"\)|SwitchIds\[(\d)\]),\s*(.+?),\s*(HatchRadius \* [0-9.]+f|[0-9.]+f),\s*(\w+)(?: \+ ([0-9.]+)f)?,\s*(K\w+),\s*true\);', points_body):
    ident = m.group(1) or ("sw_power sw_air sw_gyro sw_seal sw_arm sw_light sw_crt sw_radio".split()[int(m.group(2))])
    where, radius, reach, kind = m.group(3), (0.0 if m.group(4).startswith("Hatch") else float(m.group(4)[:-1])), consts.get(m.group(5), 0.0) + float(m.group(6) or 0), m.group(7)
    at = None
    k = re.match(r'InteractAt\(TEXT\("(\w+)"\)', where)
    kv = re.match(r'K\.Vec\(TEXT\("([^"]+)"\)', where)
    lit = re.match(r'FVector\(([-0-9.]+), ([-0-9.]+), ([-0-9.]+)\)$', where)
    if k and k.group(1) not in interact:
        continue           # (reported above)
    if kind == "KFixed" and k:
        at = interact[k.group(1)]
    elif kind == "KFixed" and kv:
        at = vec(walk(kit, kv.group(1))[0])
    elif kind == "KFixed" and where == "Couch2Seat":
        at = interact["couch_2"]
    elif kind in ("KLaunch", "KChuteHandle", "KBook") and k:
        at = interact[k.group(1)]
    elif kind == "KWheel":
        at = frame_point(door, vec(kit["hatch"]["wheel"]["in_hatch"]["at"]))
    elif kind == "KHatchEdge" and lit:
        at = frame_point(door, tuple(float(x) for x in lit.groups()))
    elif kind == "KLaunchCover" and lit:
        at = None          # two places (shut, up): below
    elif kind == "KHatchOpening":
        radius = kit["hatch"]["clear_radius"] * 0.6
        at = vec(kit["hatch"]["centre"])
    if kind == "KLaunchCover":
        for state, key in (("cover_down", "cover_closed"), ("cover_up", "cover_open")):
            targets.append((ident, frame_point(kit["panel"]["launch"][key], tuple(float(x) for x in lit.groups())), radius, reach, state))
    elif kind == "KChuteCover":
        for state, key in (("chute_cover_down", "closed"), ("chute_cover_up", "open")):
            targets.append((ident, frame_point(kit["chute"]["cover"][key], tuple(float(x) for x in lit.groups())), radius, reach, state))
    elif at is not None:
        needs = {"KLaunch": "cover_up", "KChuteHandle": "chute_cover_up", "KHatchOpening": "door_open", "KHatchEdge": "door_shut", "KWheel": "door_shut"}.get(kind, "")
        targets.append((ident, at, radius, reach, needs))


def pick(eye, look, state):
    best, best_angle = None, 1000.0
    for ident, at, radius, reach, needs in targets:
        if needs and needs not in state:
            continue
        to = add(at, eye, -1.0)
        dist = math.sqrt(sum(c * c for c in to))
        if dist > reach or dist < 1.0:
            continue
        cos = sum(a * b for a, b in zip(to, look)) / dist
        angle = math.degrees(math.acos(max(-1.0, min(1.0, cos))))
        widest = max(3.0, min(12.0, math.degrees(math.atan(radius / dist)) * 1.6 + 1.2))
        if angle <= widest and angle < best_angle:
            best, best_angle = ident, angle
    return best


def aim(eye, at):
    to = add(at, eye, -1.0)
    d = math.sqrt(sum(c * c for c in to))
    return tuple(c / d for c in to), d


seat = vec(kit["seat"]["eyes"])
standing = add(vec(kit["stand"]["centre"]), (0.0, 0.0, 165.0))
if len(targets) < 26:
    fail("cabincheck read only %d interaction targets from BuildPoints" % len(targets))
SEATED = "sw_power sw_air sw_gyro sw_seal sw_arm sw_light sw_crt sw_radio launch cover port_big port_left port_right player thermos book " \
         "chute_handle chute_cover nap".split()
STANDING = "wheel hatch sit thermos book player port_big port_left port_right".split()
for name, eye, wanted in (("the seat", seat, SEATED), ("the standing point", standing, STANDING)):
    for want in wanted:
        count("picks")
        got = []
        for ident, at, radius, reach, needs in targets:
            if ident != want:
                continue
            state = {"door_shut", needs} if needs else {"door_shut", "cover_down", "chute_cover_down"}
            look, dist = aim(eye, at)
            got.append((pick(eye, look, state), round(dist), reach))
        if not any(g[0] == want for g in got):
            fail("from %s, looking straight at %s gives %s (distance, reach: %s)" % (name, want, [g[0] for g in got], [(g[1], g[2]) for g in got]))
        elif VERBOSE:
            print("  pick from %-18s %-12s ok %s" % (name, want, got))
# neighbouring switches: half way between two of them is still one of the two, 60 % of the way is the nearer
ids = "sw_power sw_air sw_gyro sw_seal sw_arm".split()
for a, b in zip(ids, ids[1:]):
    count("picks")
    pa, pb = interact[a], interact[b]
    near_b = add(pa, add(pb, pa, -1.0), 0.6)
    got = pick(seat, aim(seat, near_b)[0], {"cover_down", "chute_cover_down", "door_shut"})
    if got != b:
        fail("from the seat, 60 %% of the way from %s to %s gives %s" % (a, b, got))

# ---------------------------------------------------------------------------------------------------------- a parse by clang
# against tools/spacecraftsim/cabinstub/CoreMinimal.h (a stand-in for the engine: what the cabin assumes of it). It
# finds typos, wrong types, const-correctness, shadowed names and unused things; it proves nothing about Unreal.
import shutil
import subprocess

if shutil.which("clang++") and "--no-parse" not in sys.argv:
    stub = os.path.join(ROOT, "Saved", "SpacecraftSim", "cabinstub")
    os.makedirs(stub, exist_ok=True)
    shutil.copy(os.path.join(ROOT, "tools", "spacecraftsim", "cabinstub", "CoreMinimal.h"), stub)
    own = {"Spacecraft/LyudSpacecraft.h", "Spacecraft/FlightModel.h"}
    for inc in set(re.findall(r'#include "([^"]+)"', cpp + header)) - own - {"CoreMinimal.h"}:
        os.makedirs(os.path.dirname(os.path.join(stub, inc)), exist_ok=True)
        with open(os.path.join(stub, inc), "w") as f:
            f.write('#include "CoreMinimal.h"\n')
    run = subprocess.run(["clang++", "-std=c++17", "-fsyntax-only", "-Wall", "-Wextra", "-Wshadow", "-Wshadow-field", "-Wno-unused-function",
                          "-Wno-mismatched-tags", "-DSuper=AActor", "-I", stub, "-I", os.path.join(SRC, "Public"), "-I", os.path.join(SRC, "Private"), CPP],
                         capture_output=True, text=True)
    found = [l for l in run.stderr.splitlines() if re.search(r'(warning|error):', l) and "LyudSpacecraft" in l]
    count("lines parsed by clang", cpp.count("\n"))
    for l in found[:40]:
        fail("clang (stub engine): " + l.replace(ROOT + os.sep, ""))
    if run.returncode != 0 and not found:
        fail("clang (stub engine) failed: " + run.stderr[-600:])
else:
    notes.append("no clang++ here (or --no-parse): the .cpp was not parsed")

# ---------------------------------------------------------------------------------------------------------- report
for n in sorted(set(notes)):
    print("note:", n)
for b in bad:
    print("CABIN", b)
print("CABINCHECK %s: %s; %d problems" % (os.path.relpath(CPP, ROOT), ", ".join("%d %s" % (v, k) for k, v in counts.items()), len(bad)))
sys.exit(1 if bad else 0)
