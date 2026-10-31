#!/bin/sh
# Extra arguments for every offscreen UnrealEditor run (-game -RenderOffscreen, commandlets) so it stays out of Dima's way:
# no mouse capture or confinement on launch or on a click (a windowless -game in the Dock used to trap the pointer in an
# invisible viewport: "No Available Windows"), no focus stealing, no sound. Use: UnrealEditor ... $("$ROOT/tools/ue_quiet_args.sh")
echo "-unattended -nosplash -nosound -NoVerifyGC" \
    "-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False" \
    "-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseCaptureMode=NoCapture" \
    "-ini:Input:[/Script/Engine.InputSettings]:DefaultViewportMouseLockMode=DoNotLock" \
    "-ini:Engine:[/Script/Engine.GameViewportClient]:bUseMouseForTouch=False"
