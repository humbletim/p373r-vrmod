#!/bin/bash
set -euo pipefail

# Usage: ./jules/screenshot.sh

EXE="$PWD/jules/build/winxtropia-custom.exe"
SNAPSHOT_DIR="_snapshot/fs-7.2.2-avx2"
EXEROOT="$(readlink -f $SNAPSHOT_DIR)/runtime"
WINEPREFIX="$PWD/jules/build/steamuser/.wine"

if [ ! -f "$EXE" ]; then
    echo "Error: Executable not found at $EXE"
    exit 1
fi

echo "Setting up environment..."

cleanup() {
    echo "Cleaning up..."
    pkill -P $$ || true
    if [ -n "${VIEWER_PID:-}" ]; then
        kill -TERM "$VIEWER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Clean logs
rm -rf "$WINEPREFIX/drive_c/users/steamuser/AppData/Roaming/SecondLife/logs/SecondLife.log"
rm -rf "$WINEPREFIX/drive_c/users/steamuser/AppData/Roaming/Firestorm_x64/logs/Firestorm.log"

export EXE
export EXEROOT
export EXE_OPTS='-set FirstLoginThisInstall 0 -nonotifications '
export WINEPATH=$EXEROOT
export USER=steamuser
export HOME="$PWD/jules/build/steamuser"
mkdir -pv $HOME
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
export WINEARCH=win64
export WINEDEBUG=-all
export WINEDLLOVERRIDES="winedbg.exe=d"
export WINEPREFIX

echo "Launching viewer..."
(
    eatmydata xvfb-run -s "-screen 0 1024x768x24 -fbdir /tmp" \
        bash -c "cd $EXEROOT ; echo \$PWD \$EXE ; fluxbox 2>&1 & wine \$EXE \$EXE_OPTS 2>&1" \
        > jules_wine.log 2>&1
) & VIEWER_PID=$!

echo "Viewer PID: $VIEWER_PID"

# Wait for log message
TIMEOUT=180
echo "Waiting up to $TIMEOUT seconds for login screen..."

export xAppData=$WINEPREFIX/drive_c/users/steamuser/AppData
LOG_FILE="$xAppData/Roaming/Firestorm_x64/logs/Firestorm.log"

# Wait for log file to appear
for i in $(seq 1 30); do
    if [ -f "$LOG_FILE" ]; then break; fi
    sleep 1
done

if timeout $TIMEOUT bash -c "tail -F $LOG_FILE 2>/dev/null | grep --line-buffered -m 1 'STATE_LOGIN_WAIT'"; then
    echo "Login screen reached!"
    sleep 2
    if convert xwd:/tmp/Xvfb_screen0 jules_screenshot.jpg; then
        echo "Screenshot saved to jules_screenshot.jpg"
    else
        echo "Failed to capture screenshot"
    fi
else
    echo "Timed out waiting for login screen."
    exit 1
fi
