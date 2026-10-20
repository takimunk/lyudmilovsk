#!/bin/sh
# Installs MPFB2 (MakeHuman for Blender) and the MakeHuman asset packs the humans are built from.
# Everything lands outside git: the extension in Blender's user extensions, the packs in MPFB's
# user data folder, downloads in blender/humans/_vendor/ (ignored).
set -e
B="${BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
HERE="$(cd "$(dirname "$0")" && pwd)"
V="$HERE/_vendor"
MPFB_TAG=v2.0.18
mkdir -p "$V/zips"
[ -d "$V/mpfb2" ] || git clone -q --depth 1 --branch $MPFB_TAG https://github.com/makehumancommunity/mpfb2.git "$V/mpfb2"
"$B" --command extension build --source-dir "$V/mpfb2/src/mpfb" --output-dir "$V"
"$B" --command extension install-file -r user_default -e "$V"/mpfb-*.zip
DATA="$("$B" -b --factory-startup --python-expr "import bpy;print('MPFBDATA', bpy.utils.extension_path_user('bl_ext.user_default.mpfb', create=True))" 2>/dev/null | sed -n 's/^MPFBDATA //p')/data"
mkdir -p "$DATA"
# files.makehumancommunity.org is much faster than files2
for p in makehuman_system_assets/makehuman_system_assets_cc0 faceunits01 visemes01 \
         hair01/hair01_cc0 hair02/hair02_ccby hair03/hair03_ccby \
         shirts01/shirts01_cc0 shirts02/shirts02_ccby shirts03/shirts03_ccby \
         pants01/pants01_cc0 pants02/pants02_ccby shoes01/shoes01_cc0 shoes02/shoes02_ccby \
         glasses01/glasses01_cc0 skins01/skins01_cc0 eyebrows01/eyebrows01_cc0 eyelashes01/eyelashes01_cc0; do
    case $p in faceunits01|visemes01) url=https://files.makehumancommunity.org/functional/$p.zip ;;
               *) url=https://files.makehumancommunity.org/asset_packs/$p.zip ;; esac
    z="$V/zips/$(basename $url)"
    [ -f "$z" ] || curl -sSL -o "$z" "$url"
    unzip -oq "$z" -d "$DATA"
done
echo "MPFB2 + asset packs installed in $DATA"
