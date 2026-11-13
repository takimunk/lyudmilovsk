#!/usr/bin/env python3
"""Checks the content paths written in the game's C++ against the files on disk, with exact case.

A packaged build reads its content from pak files and (on Linux, iOS, Android) from case-sensitive file systems,
and it only has what the cook and the staging were told to take. The editor's -game on a Mac forgives all of it:
a wrong letter case, an asset nobody references but a string, a json in a folder that is not staged.

What is checked (docs/packaging.md has the whole picture):

  1. Asset paths: every "/Game/..." and "/Engine/..." string literal in Source. A literal without a format is
     an asset, a folder or a name prefix that must exist with exactly that case. A literal with %s/%d is matched
     as a pattern against the assets on disk. LyudText::LoadTexture(TEXT("Folder"), ...) is /Game/Folder/Localized.
  2. Loose files: every "....json|.bin|.txt|.csv|.ttf|.otf|.wav|.png" literal that names a file under Content
     (by its first folder, or a bare file name found somewhere in Content).
  3. Cooking: the folder of every asset literal must be under a DirectoriesToAlwaysCook of Config/DefaultGame.ini
     (nothing but a string points at these assets, so the cooker cannot find them by itself).
  4. Staging: the folder of every loose file must be in DirectoriesToAlwaysStageAsNonUFS / AsUFS.
  5. The maps and classes named in Config/DefaultEngine.ini ([/Script/EngineSettings.GameMapsSettings]) and the
     MapsToCook of DefaultGame.ini.

Git LFS pointer files count as existing (only names are looked at, never the contents).

A path that is missing but ignored by git (the level and the content made per checkout: Content/Scene01,
Characters/Hero, Characters/Humans/Town, *_built.json) is listed as "made per checkout" and is an error only
with --strict: run with --strict on the machine that cooks, after unreal/scripts/build_scene01.sh and the imports.

Exit code: 0 when there is no problem, 1 otherwise.

A path that is missing and not ignored by git is a problem: either a typo, or content that an import script makes
and nobody committed (the cooking machine must have run that import). --names-only leaves these out and keeps
the checks that need no content: letter case against what is on disk, cooking and staging coverage.

    python3 tools/check_package_paths.py [--strict] [--names-only] [--verbose]
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(ROOT, "unreal", "Lyudmilovsk")
CONTENT = os.path.join(PROJECT, "Content")
SOURCE = os.path.join(PROJECT, "Source")
CONFIG = os.path.join(PROJECT, "Config")

LOOSE_EXT = ("json", "bin", "txt", "csv", "ttf", "otf", "wav", "png")
LOOSE_RE = re.compile(r"^[A-Za-z0-9_%][A-Za-z0-9_./%*-]*\.(?:" + "|".join(LOOSE_EXT) + ")$")
FORMAT_RE = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?(?:hs|ls|lld|llu|[sdiufc])")
LOADTEXTURE_RE = re.compile(r'LyudText::LoadTexture\(\s*TEXT\("([^"]+)"\)')
ASSET_EXT = (".uasset", ".umap")


def literals(text):
    """(line, string literal) for every "..." of a C++ source, comments skipped."""
    out = []
    i, n, line = 0, len(text), 1
    while i < n:
        c = text[i]
        if c == "\n":
            line += 1
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            line += text.count("\n", i, j)
            i = j
        elif c == "'":
            # a character literal ('"' and '\'' among them)
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
        elif c == '"':
            j = i + 1
            buf = []
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    buf.append(text[j:j + 2])
                    j += 2
                else:
                    if text[j] == "\n":
                        line += 1
                    buf.append(text[j])
                    j += 1
            out.append((line, "".join(buf)))
            i = j + 1
        else:
            i += 1
    return out


class Tree:
    """The files and folders under a root, by exact and by lower-case relative path."""

    def __init__(self, root):
        self.root = root
        self.dirs = set()
        self.files = set()
        self.lower = {}
        self.assets = []        # relative package paths (no extension): "Sea/M_LyudSea"
        self.by_name = {}       # lower-case file name -> [relative paths]
        for base, dirnames, filenames in os.walk(root):
            rel = os.path.relpath(base, root).replace(os.sep, "/")
            rel = "" if rel == "." else rel
            for d in dirnames:
                self._add(self.dirs, rel, d)
            for f in filenames:
                p = self._add(self.files, rel, f)
                self.by_name.setdefault(f.lower(), []).append(p)
                if f.endswith(ASSET_EXT):
                    self.assets.append(p[:p.rindex(".")])
        self.asset_set = set(self.assets)

    def _add(self, where, rel, name):
        p = name if not rel else rel + "/" + name
        where.add(p)
        self.lower.setdefault(p.lower(), p)
        return p

    def real_case(self, rel):
        return self.lower.get(rel.lower())


def git_ignored(rel_to_root):
    try:
        r = subprocess.run(["git", "-C", ROOT, "check-ignore", "-q", rel_to_root],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return r.returncode == 0
    except OSError:
        return False


def ini_values(path, section, key):
    """The values of every `key=` / `+key=` line of a section."""
    out = []
    if not os.path.isfile(path):
        return out
    cur = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            s = raw.strip()
            if s.startswith("[") and s.endswith("]"):
                cur = s[1:-1]
            elif cur == section and "=" in s and not s.startswith(";"):
                k, v = s.split("=", 1)
                if k.lstrip("+.") == key:
                    out.append(v.strip())
    return out


def ini_paths(path, section, key, field="Path"):
    out = []
    for v in ini_values(path, section, key):
        m = re.search(r'%s="([^"]*)"' % field, v)
        if m:
            out.append(m.group(1).rstrip("/"))
    return out


def pattern_regex(fmt, loose=False):
    """A printf format as a regex ("%s" is anything, a path with slashes too)."""
    out, pos = [], 0
    for m in FORMAT_RE.finditer(fmt):
        out.append(re.escape(fmt[pos:m.start()]))
        out.append(r"-?\d+" if m.group(0)[-1] in "diu" else r".+")
        pos = m.end()
    out.append(re.escape(fmt[pos:]))
    return "".join(out).replace(r"\*", ".*") if loose else "".join(out)


def fixed_prefix_dir(path):
    """The folders of a format before its first % ("Birds/%s/SK_%s" -> "Birds")."""
    cut = path.find("%")
    head = path if cut < 0 else path[:cut]
    return head[:head.rindex("/")] if "/" in head else ""


class Report:
    def __init__(self):
        self.errors = []
        self.generated = []
        self.notes = []
        self.ok = 0
        self.names_only = False

    def error(self, kind, where, msg):
        self.errors.append((kind, where, msg))

    def missing(self, rel_content, where, what):
        """A missing path: made per checkout when git ignores it, else an error."""
        if self.names_only:
            return
        base = "unreal/Lyudmilovsk/Content/" + rel_content
        # (a rule like "Content/Scene01/" ignores what is inside the folder, not a name that looks like it)
        if git_ignored(base) or git_ignored(base.rsplit(".", 1)[0] + "/x"):
            self.generated.append((where, what))
        else:
            self.error("MISSING", where, what)


def under(path, roots):
    return any(path == r or path.startswith(r + "/") for r in roots)


def check_asset(lit, where, game, engine_cook, cook_dirs, rep, covered):
    """One "/Game/..." or "/Engine/..." literal."""
    if lit.startswith("/Engine/"):
        # engine content is not in this repository: only the cooking is checked
        d = lit[:lit.rindex("/")] if "%" not in lit else "/Engine/" + fixed_prefix_dir(lit[len("/Engine/"):])
        if not under(d, engine_cook):
            covered.setdefault(d, []).append(where)
        else:
            rep.ok += 1
        return
    path = lit[len("/Game/"):]
    if path.startswith("%"):
        return                       # "/Game/%s/Localized/...": checked through its LoadTexture callers
    has_fmt = "%" in path
    prefix_dir = fixed_prefix_dir(path) if has_fmt else None

    # --- existence, exact case
    found = None                      # the content-relative folder that must be cooked
    if not has_fmt:
        clean = path.rstrip("/")
        pkg = clean.split(".", 1)[0]
        if "." in clean and clean.split(".", 1)[1] != pkg.rsplit("/", 1)[-1]:
            rep.error("BAD PATH", where, "%s: the object name differs from the package name" % lit)
        if pkg in game.asset_set:
            found = pkg.rsplit("/", 1)[0] if "/" in pkg else ""
        elif clean in game.dirs:
            found = clean
        else:
            parent, _, stem = pkg.rpartition("/")
            is_prefix = "." not in clean and parent in game.dirs and any(
                a.startswith(pkg) for a in game.assets if a.rpartition("/")[0] == parent)
            real = game.real_case(pkg + ".uasset") or game.real_case(pkg + ".umap") or game.real_case(clean)
            if is_prefix:
                found = parent
            elif real:
                rep.error("CASE", where, "%s: on disk it is %s" % (lit, real))
                return
            else:
                rep.missing(pkg + ".uasset", where, lit)
                found = None
                # the cooking of its folder is still checked below
                prefix_dir = parent if "." in clean or not clean.endswith("/") else clean
                if path.endswith("/"):
                    prefix_dir = clean
    else:
        rx = pattern_regex(path)
        objectish = "." in path[path.rfind("/") + 1:]
        if objectish:
            cands = (a + "." + a.rsplit("/", 1)[-1] for a in game.assets)
            full = re.compile("^" + rx + "$")
        else:
            cands = list(game.assets) + list(game.dirs)
            full = re.compile("^" + rx)          # a package, a folder or a name prefix
        cands = list(cands)
        if any(full.match(c) for c in cands):
            found = prefix_dir
        else:
            fold = re.compile(full.pattern, re.IGNORECASE)
            hit = next((c for c in cands if fold.match(c)), None)
            if hit:
                rep.error("CASE", where, "%s: nothing matches with this case, on disk there is %s" % (lit, hit))
                return
            real = game.real_case(prefix_dir) if prefix_dir else ""
            if prefix_dir and prefix_dir not in game.dirs and real:
                rep.error("CASE", where, "%s: the folder on disk is %s" % (lit, real))
                return
            rep.missing((prefix_dir + "/x.uasset") if prefix_dir else "x.uasset", where,
                        "%s: no asset on disk matches the pattern" % lit)

    # --- cooking
    folder = found if found is not None else prefix_dir
    if folder is None:
        return
    game_path = "/Game/" + folder if folder else "/Game"
    if under(game_path, cook_dirs):
        if found is not None:
            rep.ok += 1
    else:
        top = "/".join(game_path.split("/")[:3])
        covered.setdefault(top, []).append(where)


def check_loose(lit, where, game, stage_dirs, rep, top_dirs_lower, verbose):
    if ".." in lit or lit.startswith(("http", "/")):
        return
    has_fmt = "%" in lit or "*" in lit
    first = lit.split("/", 1)[0]
    if "/" in lit and first.lower() in top_dirs_lower:
        rel = lit
    elif "/" not in lit:
        # a bare file name: joined with a folder in code. Found by name anywhere under Content.
        if has_fmt:
            rx = re.compile("^" + pattern_regex(lit, True) + "$")
            names = [p for ps in game.by_name.values() for p in ps if "/" in p]
            hits = [p for p in names if rx.match(p.rsplit("/", 1)[-1])]
            if not hits:
                fold = re.compile(rx.pattern, re.IGNORECASE)
                near = [p for p in names if fold.match(p.rsplit("/", 1)[-1])]
                if near:
                    rep.error("CASE", where, '"%s": nothing matches with this case, on disk there is %s' % (lit, near[0]))
                elif verbose:
                    rep.notes.append((where, '"%s": no such file under Content (an output file?)' % lit))
                return
        else:
            hits = [p for p in game.by_name.get(lit.lower(), [])]
            exact = [p for p in hits if p.rsplit("/", 1)[-1] == lit]
            if hits and not exact:
                rep.error("CASE", where, '"%s": on disk it is %s' % (lit, hits[0]))
                return
            hits = exact
            if not hits:
                if verbose:
                    rep.notes.append((where, '"%s": no such file under Content (an output file?)' % lit))
                return
        for p in hits:
            top = p.split("/", 1)[0]
            if top not in stage_dirs:
                rep.error("NOT STAGED", where, '"%s" (Content/%s): add +DirectoriesToAlwaysStageAsNonUFS=(Path="%s")' % (lit, p, top))
                return
        rep.ok += 1
        return
    else:
        if verbose:
            rep.notes.append((where, '"%s": not under Content (Saved or an output file?)' % lit))
        return

    # a path from Content's root
    if has_fmt:
        rx = re.compile("^" + pattern_regex(rel, True) + "$")
        if not any(rx.match(f) for f in game.files):
            fold = re.compile(rx.pattern, re.IGNORECASE)
            near = next((f for f in game.files if fold.match(f)), None)
            if near:
                rep.error("CASE", where, '"%s": nothing matches with this case, on disk there is Content/%s' % (lit, near))
                return
            rep.missing(fixed_prefix_dir(rel) + "/x", where, '"%s": no file on disk matches the pattern' % lit)
    elif rel not in game.files:
        real = game.real_case(rel)
        if real:
            rep.error("CASE", where, '"%s": on disk it is Content/%s' % (lit, real))
            return
        rep.missing(rel, where, '"Content/%s"' % rel)
    top = game.real_case(first) or first
    if top not in stage_dirs:
        rep.error("NOT STAGED", where, '"%s": add +DirectoriesToAlwaysStageAsNonUFS=(Path="%s") to DefaultGame.ini' % (lit, top))
    else:
        rep.ok += 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--strict", action="store_true", help="content made per checkout (ignored by git) must exist too")
    ap.add_argument("--names-only", action="store_true",
                    help="letter case, cooking and staging only: say nothing about paths that do not exist at all "
                         "(for a checkout without the imported content)")
    ap.add_argument("--verbose", action="store_true", help="also list the file literals that are not under Content")
    args = ap.parse_args()

    game = Tree(CONTENT)
    top_dirs_lower = {d.lower() for d in game.dirs if "/" not in d}
    game_ini = os.path.join(CONFIG, "DefaultGame.ini")
    engine_ini = os.path.join(CONFIG, "DefaultEngine.ini")
    pack = "/Script/UnrealEd.ProjectPackagingSettings"
    cook_all = ini_paths(game_ini, pack, "DirectoriesToAlwaysCook")
    cook_dirs = [d for d in cook_all if d.startswith("/Game")]
    engine_cook = [d for d in cook_all if d.startswith("/Engine")]
    stage_dirs = set(ini_paths(game_ini, pack, "DirectoriesToAlwaysStageAsNonUFS") +
                     ini_paths(game_ini, pack, "DirectoriesToAlwaysStageAsUFS"))
    rep = Report()
    rep.names_only = args.names_only
    uncovered = {}

    # --- the config itself
    for d in cook_dirs:
        rel = d[len("/Game/"):] if d != "/Game" else ""
        if rel and rel not in game.dirs:
            real = game.real_case(rel)
            if real:
                rep.error("CASE", "Config/DefaultGame.ini", "DirectoriesToAlwaysCook %s: on disk it is %s" % (d, real))
            else:
                rep.missing(rel + "/x.uasset", "Config/DefaultGame.ini", "DirectoriesToAlwaysCook %s: no such folder" % d)
    for d in sorted(stage_dirs):
        if d not in game.dirs:
            real = game.real_case(d)
            if real:
                rep.error("CASE", "Config/DefaultGame.ini", "DirectoriesToAlwaysStage %s: on disk it is %s" % (d, real))
            else:
                rep.missing(d + "/x", "Config/DefaultGame.ini", "DirectoriesToAlwaysStage %s: no such folder" % d)
    maps = ini_paths(game_ini, pack, "MapsToCook", "FilePath")
    section = "/Script/EngineSettings.GameMapsSettings"
    for key in ("GameDefaultMap", "EditorStartupMap", "GlobalDefaultGameMode", "TransitionMap"):
        for v in ini_values(engine_ini, section, key):
            if v.startswith("/Game/"):
                maps.append((key, v))
    if not ini_paths(game_ini, pack, "MapsToCook", "FilePath"):
        rep.error("NO MAPS", "Config/DefaultGame.ini", "no +MapsToCook: without it (and without -map=) the cooker takes every map under Content")
    for m in maps:
        key, v = m if isinstance(m, tuple) else ("MapsToCook", m)
        pkg = v[len("/Game/"):].split(".", 1)[0]
        if pkg not in game.asset_set:
            real = game.real_case(pkg + ".umap") or game.real_case(pkg + ".uasset")
            if real:
                rep.error("CASE", "Config", "%s=%s: on disk it is %s" % (key, v, real))
            else:
                rep.missing(pkg + ".umap", "Config", "%s=%s" % (key, v))
        else:
            rep.ok += 1

    # --- the source
    for base, _, names in os.walk(SOURCE):
        for name in sorted(names):
            if not name.endswith((".cpp", ".h")):
                continue
            full = os.path.join(base, name)
            rel_src = os.path.relpath(full, PROJECT).replace(os.sep, "/")
            with open(full, encoding="utf-8", errors="replace") as f:
                text = f.read()
            for m in LOADTEXTURE_RE.finditer(text):
                line = text.count("\n", 0, m.start()) + 1
                check_asset("/Game/%s/Localized/" % m.group(1), "%s:%d" % (rel_src, line), game, engine_cook, cook_dirs, rep, uncovered)
            for line, lit in literals(text):
                where = "%s:%d" % (rel_src, line)
                if lit.startswith(("/Game/", "/Engine/")):
                    check_asset(lit, where, game, engine_cook, cook_dirs, rep, uncovered)
                elif LOOSE_RE.match(lit):
                    check_loose(lit, where, game, stage_dirs, rep, top_dirs_lower, args.verbose)

    for top in sorted(uncovered):
        places = uncovered[top]
        rep.error("NOT COOKED", places[0], '%s is loaded by path in %d place(s) and is under no DirectoriesToAlwaysCook: add +DirectoriesToAlwaysCook=(Path="%s")'
                  % (top, len(places), top))

    # --- the report
    if rep.generated:
        print("Made per checkout (ignored by git, absent here)%s:" % (" - an error with --strict" if args.strict else ""))
        seen = set()
        for where, what in rep.generated:
            if what not in seen:
                seen.add(what)
                print("  %-52s %s" % (where, what))
        print()
    if rep.notes:
        print("Notes:")
        for where, what in rep.notes:
            print("  %-52s %s" % (where, what))
        print()
    problems = len(rep.errors) + (len(rep.generated) if args.strict else 0)
    if rep.errors:
        print("Problems:")
        for kind, where, msg in sorted(rep.errors):
            print("  %-10s %-52s %s" % (kind, where, msg))
        print()
    print("check_package_paths: %d paths fine, %d made per checkout, %d problem(s)" % (rep.ok, len(rep.generated), problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
