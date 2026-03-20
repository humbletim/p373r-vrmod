#!/bin/bash
set -euo pipefail

XWIN_VER=0.8.0
SDKVER=10.0.26100
CRTVER=14.44.17.14
VARIANTS=desktop
SDKROOT=winsdk
XWIN_URL=https://github.com/Jake-Shadle/xwin/releases/download/$XWIN_VER/xwin-$XWIN_VER-x86_64-unknown-linux-musl.tar.gz

wget -nv "$XWIN_URL"
mkdir -pv bin
tar -C bin --wildcards --strip-components=1 -xvf xwin-$XWIN_VER-x86_64-unknown-linux-musl.tar.gz \*/xwin
chmod a+x bin/xwin
mkdir -pv tmp
bin/xwin --accept-license -Loff --cache-dir=tmp --variant $VARIANTS --crt-version $CRTVER --sdk-version $SDKVER splat --output $SDKROOT

declare -xp XWIN_VER XWIN_URL SDKVER CRTVER SDKROOT | tee $SDKROOT/readme.env
