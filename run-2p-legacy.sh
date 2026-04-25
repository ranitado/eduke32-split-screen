#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

grp_file=""
for candidate in duke3d.grp DUKE3D.GRP; do
    if [[ -f "$candidate" ]]; then
        grp_file="$candidate"
        break
    fi
done

if [[ -z "$grp_file" ]]; then
    echo "No se encontro duke3d.grp o DUKE3D.GRP en la carpeta del proyecto." >&2
    exit 1
fi

export TMPDIR="${TMPDIR:-/tmp}"

exec ./eduke32 \
    -usecwd \
    -gamegrp "$grp_file" \
    -mx package/sdk/samples/splitscr_nativepads.con \
    -q2 \
    "$@"
