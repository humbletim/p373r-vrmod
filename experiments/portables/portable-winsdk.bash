#!/bin/bash

set -euo pipefail

XWIN_VER=0.6.7+vfsoverlay
SDKVER=10.0.26100
CRTVER=14.44.17.14
SDKROOT=winsdk

version_lt() {
    [[ "$1" == "$2" ]] && return 1
    [[ "$1" == "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -n 1)" ]]
}

if test ! -v skipxwincheck && version_lt `bin/xwin --version tool | awk '{print $NF}'` $XWIN_VER ; then
  echo "nope $(bin/xwin --version) != $XWIN_VER" >&2
  exit 1
fi

accept=
test -v GITHUB_ACTIONS && accept=--accept-license

bin/xwin $accept -Loff --cache-dir=tmp --crt-version $CRTVER --sdk-version $SDKVER download
bin/xwin --accept-license -Loff --cache-dir=tmp --crt-version $CRTVER --sdk-version $SDKVER splat --vfsoverlay --output $SDKROOT

declare -xp XWIN_VER SDKVER CRTVER SDKROOT | tee $SDKROOT/readme.env
