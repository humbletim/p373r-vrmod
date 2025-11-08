#!/bin/bash
set -euo pipefail

# test ! -v CONTY_WINE || bash conty.bash

if ! which wine && test -v GITHUB_ACTIONS ; then
    echo "installing wine..." >&2
    sudo bash -c 'eatmydata apt install -y -qq -o=Dpkg::Use-Pty=0 wine-stable xvfb fluxbox imagemagick 2>&1 ' | grep -i 'install'
fi

VIEWER_PID=
# export PATH=$PATH:$PWD/build/wine/bin

# This function will be called on script EXIT (normal or error)
cleanup() {
    test -n "$VIEWER_PID" || return 0
    set +eu
    echo "Cleaning up..."
    kill -TERM -"$VIEWER_PID" 2>/dev/null || true
    sleep 0.25
    ps -eo pid,ppid,pgid,cmd --forest | grep -E "Xvfb|winxtropia|wine|[.]exe" || true
    kill -KILL -"$VIEWER_PID" 2>/dev/null || true
    # This kills the wineserver, winedevice, etc. that have PPID 1
    echo "Hunting for any detached wine processes..."
    if test -v GITHUB_ACTIONS ; then
        pkill -f "wineserver" 2>/dev/null || true
        pkill -f "Xvfb" 2>/dev/null || true
        pkill -f ".exe" 2>/dev/null || true
    fi
    VIEWER_PID=
    echo "Cleanup complete."
}

# Set the trap *once* at the beginning.
trap cleanup EXIT

# Ensure required variables are set
test -v base || { echo 'Error: $base variable not set.' ; exit 1 ; }
test -v snapshot_dir || { echo 'Error: $snapshot_dir variable not set.' ; exit 1 ; }

# Check for dependencies
which xvfb-run || { echo 'Error: xvfb-run not found in $PATH.' ; exit 1 ; }
which convert || { echo 'Error: ImageMagick "convert" not found in $PATH.' ; exit 1 ; }
which wine || { echo 'Error: "wine" not found in $PATH.' ; exit 1 ; }

    if test -v GITHUB_ACTIONS ; then
        sudo pkill -f .exe || true
        sudo pkill -f wine || true
        sudo pkill -f xvfb-run || true
        sudo pkill -f Xvfb || true
    fi
# rm -rf ~/.wine || true
#  rm ~/.wine/.update-timestamp || true
# # # export WINEDEBUG=-all
# timeout 30 xvfb-run --auto-servernum --server-num=1 build/wine/bin/wine64 cmd /c help || true
# # # pkill -f .exe || true
# rm ~/.wine/.update-timestamp|| true
# DISPLAY=:99 timeout 5 build/wine/bin/wine cmd /c echo ok || true
# rm ~/.wine/.update-timestamp|| true

set -m # <--- ADD THIS (Monitor Mode)

#export WINEDEBUG=-all
#export WINEDLLOVERRIDES="winedbg.exe=d"
echo "Starting background viewer..."
# which is also the PGID for itself and all its children (wine, .exe).
# bash -c also starts a new process group. $! gives us the PID of bash,
#
# !!! REMOVED 'nohup' from inside this string !!!
#
export WINEPREFIX="$PWD/build/steamuser/.wine"
(
    export EXE="$PWD/build/winxtropia-vrmod.$base.exe"
    export EXEROOT="$(readlink -f $snapshot_dir)/runtime"
    export EXE_OPTS='-set FirstLoginThisInstall 0 -nonotifications '
    export WINEPATH=$EXEROOT

    export USER=steamuser
    export HOME="$PWD/build/steamuser"
    mkdir -pv $HOME
    export XDG_DATA_HOME="$HOME/.local/share"
    export XDG_CONFIG_HOME="$HOME/.config"
    export XDG_CACHE_HOME="$HOME/.cache"
    export WINEARCH=win64
    export WINEDEBUG=-all
    export WINEDLLOVERRIDES="winedbg.exe=d"

    # set -x
    eatmydata xvfb-run -s "-screen 0 1024x768x24 -fbdir /tmp" bash -c "cd $EXEROOT ; echo $PWD $EXE ; fluxbox 2>&1 & wine $EXE $EXE_OPTS 2>&1" 2>&1 > wine.log
) & VIEWER_PID=$!

# echo HOME=$HOME
# sleep 2

sleep 1.0 # Give it a moment

if test -v VIEWER_PID && ps -p $VIEWER_PID >/dev/null; then
    echo "Launched viewer process group: $VIEWER_PID"
else
    echo "Error: viewer (PID ${VIEWER_PID:-}) died immediately."
    exit 22
fi

N=60
echo "Viewer(pgid ${VIEWER_PID:-})... waiting $N seconds for app to load..."

# This command will:
# 1. Tail the log files.
# 2. Grep for *all* lines containing "STATE_" (for general logging).
# 3. Use 'tee' to print those "STATE_" lines to stderr (so you see them in the CI log).
# 4. Pass those same lines to a *second* grep, which blocks until it sees "STATE_LOGIN_WAIT" (the readiness signal).
# 5. The 'grep -m 1' will exit with success (0) as soon as it finds the line, ending the 'timeout'.
# 6. If "STATE_LOGIN_WAIT" is *not* found, 'timeout' will kill the pipe after $N seconds.
# 7. '|| true' ensures we don't fail the build if it times out (we'll just get a "stuck" screenshot).
sleep 1
export xAppData=$WINEPREFIX/drive_c/users/steamuser/AppData
# mkdir -pv $AppData/Roaming/Firestorm_x64/logs
# mkdir -pv $AppData/Roaming/SecondLife/logs
# touch /home/runner/.wine/drive_c/users/runner/AppData/Roaming/Firestorm_x64/logs/Firestorm.log
# touch /home/runner/.wine/drive_c/users/runner/AppData/Roaming/SecondLife/logs/SecondLife.log
# truncate -s 0 $AppData/Roaming/Firestorm_x64/logs/Firestorm.log
# truncate -s 0 $AppData/Roaming/SecondLife/logs/SecondLife.log
timeout $N bash -c 'tail -F $xAppData/Roaming/SecondLife/logs/SecondLife.log $xAppData/Roaming/Firestorm_x64/logs/Firestorm.log 2>/dev/null | grep --line-buffered "STATE_" | tee /dev/stderr | { grep --color=always --line-buffered -m 1 "STATE_LOGIN_WAIT" && pkill -P $$ tail ; }' || true
sleep 1

OK=$(grep STATE_LOGIN_WAIT $xAppData/Roaming/*/logs/*.log || true)
 
if [[ -n "$OK" ]] ; then 
    echo "App reached login screen state!"
    #DISPLAY=:99 convert xwd:/tmp/Xvfb_screen0 screencapture_raw.jpg
    #DISPLAY=:99 xwd -root -silent | convert xwd:- screencapture.jpg ; then

    if convert xwd:/tmp/Xvfb_screen0 screencapture.jpg ; then
        echo "Screenshot captured successfully:"
        ls -lrth screencapture.jpg
    else
        echo "Error: Failed to capture screenshot."
        # Let the script exit; trap will handle cleanup.
        exit 30
    fi
    cleanup
    # Check if the upload script exists
    if [[ -v GITHUB_ACTIONS && -f "./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash" ]]; then
        if [[ -v INPUT_TMATE ]] ; then
            echo "... uploading screenshot in 5 seconds (hit control-c to cancel)"
            sleep 5
        fi

        # Source it
        . ./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash
        # Run the upload
        gha-upload-artifact-fast screencapture.jpg screencapture.jpg 1 9 true
    else
        echo "Warning: Upload script not found. Skipping upload."
    fi
else
    echo "App did not reach login screen... $N have passed."
fi

cleanup

echo "Script finished."
# The 'trap' will now execute automatically as the script exits.