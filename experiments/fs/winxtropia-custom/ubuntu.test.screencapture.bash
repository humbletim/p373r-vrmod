#!/bin/bash
set -euo pipefail

test -v base || { echo '!base' ; exit 1 ; }
test -v snapshot_dir || { echo '!snapshot_dir' ; exit 1 ; }

which Xvfb || { echo '!Xvfb' ; exit 1 ; } 
timeout 60 Xvfb :99 -screen 0 1024x768x24 -fbdir /tmp &
bash -c 'cd $snapshot_dir/runtime && env DISPLAY=:99 timeout 30 wine ../../winxtropia-vrmod.$base.exe' &
sleep 20
convert xwd:/tmp/Xvfb_screen0 screencapture.jpg && ls -lrth screencapture.jpg
sleep 5
. ./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash
gha-upload-artifact-fast screencapture.jpg screencapture.jpg 1 9 true
