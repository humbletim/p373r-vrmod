#!/bin/bash
set -euo pipefail

test -v snapshot_dir
test -v base

timeout 60 Xvfb :99 -screen 0 1024x768x24 -fbdir /tmp &
bash -c 'cd $snapshot_dir/runtime && env DISPLAY=:99 timeout 30 wine ../../winxtropia-vrmod.$base.exe' &
sleep 20
convert xwd:/tmp/Xvfb_screen0 screencapture.jpg && ls -lrth screencapture.jpg
. ./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash
gha-upload-artifact-fast screencapture.jpg screencapture.jpg 1 9
