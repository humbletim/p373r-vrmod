#!/bin/bash
set -euo pipefail

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

    if [[ -v VIEWER_PID ]]; then
        echo "Cleaning up background viewer process group $VIEWER_PID..."
        # Kill the entire process group headed by bash -c
        kill -TERM -"$VIEWER_PID" 2>/dev/null || true
    fi

    if [[ -v XVFB_PID ]]; then
        echo "Cleaning up background Xvfb process group $XVFB_PID..."
        # Kill the entire process group headed by nohup
        kill -TERM -"$XVFB_PID" 2>/dev/null || true
    fi

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

export EXE="$PWD/build/winxtropia-vrmod.$base.exe"

echo "Starting background viewer..."
# bash -c also starts a new process group. $! gives us the PID of bash,
# which is also the PGID for itself and all its children (wine, .exe).
bash -c 'set -x ; cd "$0" && DISPLAY=:99 nohup wine "$1"' "$snapshot_dir/runtime" "$EXE" > wine.log & VIEWER_PID=$!
sleep 1.0 # Give it a moment

if ps -p $VIEWER_PID >/dev/null; then
    echo "Launched viewer process group: $VIEWER_PID"
else
    echo "Error: viewer (PID $VIEWER_PID) died immediately."
    exit 22
fi

echo "Xvfb(pgid $XVFB_PID) Viewer(pgid $VIEWER_PID)... waiting 20 seconds for app to load..."
sleep 20

echo "Attempting to capture screenshot..."
# Note: Added -display :99 to convert, just in case
if convert xwd:/tmp/Xvfb_screen0 screencapture.jpg; then
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
