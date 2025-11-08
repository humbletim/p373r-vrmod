#!/bin/bash
set -euo pipefail
set -m # <--- ADD THIS (Monitor Mode)

test ! -x build/wine/bin/wine && test -v CONTY_WINE && {
    echo "installing conty_wine..." >&2
    sudo bash -c 'eatmydata apt install -y -qq -o=Dpkg::Use-Pty=0 xvfb fluxbox imagemagick nano fuse 2>&1 ' > /dev/null
    mkdir -pv build
    wget -P build https://github.com/Kron4ek/Conty/releases/download/1.28.3/conty_wine_dwarfs.sh
    bash -c 'cd build && chmod a+x conty_wine_dwarfs.sh && ./conty_wine_dwarfs.sh -e && ln -s conty_wine_dwarfs.sh_files wine'
    ./build/wine/bin/wine --version
} || true

# This function will be called on script EXIT (normal or error)
cleanup() {
    echo "Cleaning up..."

    # --- The "Brutally Effective" Cleanup ---
    # We send SIGTERM to the *entire process group* by using the negative PID.
    # This ensures that the leader process (bash/nohup) AND all its
    # descendants (wine, the .exe, Xvfb) are all terminated.
    #
    # We use "|| true" to prevent the script from exiting with an error
    # if the process is already dead (e.g., it crashed).

    # --- Stage 1: Polite SIGTERM ---
    if [[ -v VIEWER_PID ]]; then
        echo "Cleaning up background viewer process group $VIEWER_PID (TERM)..."
        # Kill the entire process group headed by bash -c
        kill -TERM -"$VIEWER_PID" 2>/dev/null || true
    fi

    if [[ -v XVFB_PID ]]; then
        echo "Cleaning up background Xvfb process group $XVFB_PID (TERM)..."
        # Kill the entire process group headed by nohup
        kill -TERM -"$XVFB_PID" 2>/dev/null || true
    fi

    sleep 0.5 # Give them a moment to die

    ps -eo pid,ppid,pgid,cmd --forest | grep -E "Xvfb|winxtropia|wine|bash" || true
    # --- Stage 2: Brutal SIGKILL ---
    # This handles any process ignoring SIGTERM
    if [[ -v VIEWER_PID ]]; then
        echo "Ensuring viewer group $VIEWER_PID is gone (KILL)..."
        kill -KILL -"$VIEWER_PID" 2>/dev/null || true
    fi

    if [[ -v XVFB_PID ]]; then
        echo "Ensuring Xvfb group $XVFB_PID is gone (KILL)..."
        kill -KILL -"$XVFB_PID" 2>/dev/null || true
    fi

    # --- Stage 3: Hunt the detached daemons ---
    # This kills the wineserver, winedevice, etc. that have PPID 1
    echo "Hunting for any detached wine processes..."
    pkill -f "wineserver" 2>/dev/null || true
    pkill -f "winedevice.exe" 2>/dev/null || true
    pkill -f "winxtropia-vrmod" 2>/dev/null || true

    echo "Cleanup complete."
}
# Set the trap *once* at the beginning.
trap cleanup EXIT

# Ensure required variables are set
test -v base || { echo 'Error: $base variable not set.' ; exit 1 ; }
test -v snapshot_dir || { echo 'Error: $snapshot_dir variable not set.' ; exit 1 ; }

# Check for dependencies
which Xvfb || { echo 'Error: Xvfb not found in $PATH.' ; exit 1 ; }
which convert || { echo 'Error: ImageMagick "convert" not found in $PATH.' ; exit 1 ; }
which wine || { echo 'Error: "wine" not found in $PATH.' ; exit 1 ; }

echo "Starting background Xvfb..."
# nohup starts a new process group. $! gives us the PID of nohup, which is
# also the PGID (Process Group ID) for itself and its child (Xvfb).
nohup Xvfb :99 -screen 0 1024x768x24 -fbdir /tmp > Xvfb.log & XVFB_PID=$!
sleep 1.0 # Give it a moment to start or fail

# Check if the process is still running
if ps -p $XVFB_PID >/dev/null; then
    echo "Launched Xvfb process group: $XVFB_PID"
else
    echo "Error: Command Xvfb (PID $XVFB_PID) died immediately."
    # We don't need to 'exit 19' here, the trap will handle cleanup
    # and the 'set -e' would kill the script, but we'll be explicit.
    exit 19
fi

pkill -f .exe || true
rm ~/.wine/.update-timestamp
# export WINEDEBUG=-all
DISPLAY=:99 build/wine/usr/bin/wine cmd /c echo ok < /dev/null || true
# pkill -f .exe || true

export EXE="$PWD/build/winxtropia-vrmod.$base.exe"
export EXEROOT="$(readlink -f $snapshot_dir)/runtime"
export WINEPATH=$EXEROOT
export WINEDEBUG=-all
export WINEDLLOVERRIDES="winedbg.exe=d"
echo "Starting background viewer..."
# bash -c also starts a new process group. $! gives us the PID of bash,
# which is also the PGID for itself and all its children (wine, .exe).
#
# !!! REMOVED 'nohup' from inside this string !!!
#
bash -c 'set -x ; export DISPLAY=:99 ; cd "$0" && fluxbox & build/wine/bin/wine "$1" -set FSShowWhitelistReminder 0 -set FirstLoginThisInstall 0' "$EXEROOT" "$EXE" > wine.log 2>&1 & VIEWER_PID=$!
sleep 1.0 # Give it a moment

if ps -p $VIEWER_PID >/dev/null; then
    echo "Launched viewer process group: $VIEWER_PID"
else
    echo "Error: viewer (PID $VIEWER_PID) died immediately."
    exit 22
fi

N=60
echo "Xvfb(pgid $XVFB_PID) Viewer(pgid $VIEWER_PID)... waiting $N seconds for app to load..."

# This command will:
# 1. Tail the log files.
# 2. Grep for *all* lines containing "STATE_" (for general logging).
# 3. Use 'tee' to print those "STATE_" lines to stderr (so you see them in the CI log).
# 4. Pass those same lines to a *second* grep, which blocks until it sees "STATE_LOGIN_WAIT" (the readiness signal).
# 5. The 'grep -m 1' will exit with success (0) as soon as it finds the line, ending the 'timeout'.
# 6. If "STATE_LOGIN_WAIT" is *not* found, 'timeout' will kill the pipe after $N seconds.
# 7. '|| true' ensures we don't fail the build if it times out (we'll just get a "stuck" screenshot).
sleep 1
AppData=/home/runner/.wine/drive_c/users/steamuser/AppData
mkdir -pv $AppData/Roaming/Firestorm_x64/logs
mkdir -pv $AppData/Roaming/SecondLife/logs
# touch /home/runner/.wine/drive_c/users/runner/AppData/Roaming/Firestorm_x64/logs/Firestorm.log
# touch /home/runner/.wine/drive_c/users/runner/AppData/Roaming/SecondLife/logs/SecondLife.log
truncate -s 0 $AppData/Roaming/Firestorm_x64/logs/Firestorm.log
truncate -s 0 $AppData/Roaming/SecondLife/logs/SecondLife.log
timeout $N bash -c 'set -x ; tail -F $AppData/Roaming/*/logs/*.log 2>/dev/null | grep --line-buffered "STATE_" | tee /dev/stderr | { grep --color=always --line-buffered -m 1 "STATE_LOGIN_WAIT" && pkill -P $$ tail ; }' || true
sleep 1

grep STATE_LOGIN_WAIT $AppData/Roaming/*/logs/*.log

echo "App is either ready or $N have passed. Attempting to capture screenshot..."

#DISPLAY=:99 convert xwd:/tmp/Xvfb_screen0 screencapture_raw.jpg
#DISPLAY=:99 xwd -root -silent | convert xwd:- screencapture.jpg ; then

if DISPLAY=:99 convert xwd:/tmp/Xvfb_screen0 screencapture.jpg ; then
    echo "Screenshot captured successfully:"
    ls -lrth screencapture.jpg
else
    echo "Error: Failed to capture screenshot."
    # Let the script exit; trap will handle cleanup.
    exit 30
fi

sleep 1
# NOTE: We call 'cleanup' here because at this point done with the capture
cleanup

# Check if the upload script exists
if [[ -f "./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash" ]]; then
    echo "... uploading screenshot in 5 seconds (hit control-c to cancel)"
    sleep 5

    # Source it
    . ./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash
    # Run the upload
    gha-upload-artifact-fast screencapture.jpg screencapture.jpg 1 9 true
else
    echo "Warning: Upload script not found. Skipping upload."
fi

echo "Script finished."
# The 'trap' will now execute automatically as the script exits.