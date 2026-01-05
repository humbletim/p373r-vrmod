#!/bin/bash
export EXE=/app/asdf/fs-7.2.2-avx2.exe
export EXE_OPTS="-set FirstLoginThisInstall 0 -nonotifications"
# Note: "Test User" must be quoted correctly
export SCREENSHOT_OPTS="--grid 127.0.0.1:9000 --login Test User password"
export WINEPREFIX=/app/asdf/steamuser/.wine
export WINEPATH=/opt/humbletim/_snapshot/fs-7.2.2-avx2/runtime
export USER=steamuser
export HOME=/app/asdf/steamuser
mkdir -p $HOME
export XDG_DATA_HOME=$HOME/.local/share
export XDG_CONFIG_HOME=$HOME/.config
export XDG_CACHE_HOME=$HOME/.cache
export WINEARCH=win64
export WINEDEBUG=-all
export WINEDLLOVERRIDES=winedbg.exe=d

export DISPLAY=:99
Xvfb :99 -screen 0 1024x768x24 -fbdir /tmp -nolisten tcp &
XVFB_PID=$!
echo "Xvfb started with PID $XVFB_PID"
sleep 2

fluxbox &
echo "Fluxbox started"

cd /opt/humbletim/_snapshot/fs-7.2.2-avx2/runtime

# Using array for arguments to handle spaces safely if needed, but here simple string expansion works for wine?
# Wine arguments handling is tricky.
# We will pass the arguments as a single string to bash/wine invocation or rely on shell splitting.
# "--login Test User password" -> Test User are 2 args? No, login takes First Last Password.
# So --login "Test" "User" "password".
# Firestorm expects: --login <firstname> <lastname> <password>
# So arguments are: --login, Test, User, password.
# So I should NOT quote "Test User" as one argument?
# Standard SL viewer arg: --login <first> <last> <pass>
# So: --login Test User password

wine "$EXE" $EXE_OPTS --grid 127.0.0.1:9000 --login Test User password &
VIEWER_PID=$!
echo "Viewer started with PID $VIEWER_PID"

echo "Waiting 90 seconds..."
sleep 90

echo "Taking screenshot..."
import -window root /app/asdf/screenshot-region.jpg
ls -l /app/asdf/screenshot-region.jpg

echo "Killing viewer..."
kill $VIEWER_PID
sleep 5
kill $XVFB_PID
echo "Done."
