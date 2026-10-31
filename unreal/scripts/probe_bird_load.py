"""Editor memory taken by loading every bird species' skeletal bodies, as BirdLife's setup did in PIE.
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nosplash
Prints BIRDLOAD lines: process footprint before and after loading SK_<Id> and SK_<Id>_Hero of every species."""
import os
import subprocess
import unreal


def footprint_mb():
    out = subprocess.run(["footprint", "-p", str(os.getpid())], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if "Footprint:" in line:
            p = line.split()
            i = p.index("Footprint:")
            v, u = float(p[i + 1]), p[i + 2]
            return v * 1024 if u == "GB" else v / 1024 if u == "KB" else v
    return -1


ids = sorted(d for d in os.listdir(unreal.Paths.project_content_dir() + "Birds")
             if os.path.exists(unreal.Paths.project_content_dir() + "Birds/%s/SK_%s.uasset" % (d, d)))
unreal.log_warning("BIRDLOAD %d species, start %.0f MB" % (len(ids), footprint_mb()))
keep = []
for suffix in ("", "_Hero"):
    for i in ids:
        a = unreal.load_asset("/Game/Birds/%s/SK_%s%s" % (i, i, suffix))
        if a:
            keep.append(a)
    unreal.log_warning("BIRDLOAD after SK_*%s: %d loaded, %.0f MB" % (suffix, len(keep), footprint_mb()))
unreal.log_warning("BIRDLOAD done")
