#!/bin/bash
set -euo pipefail
test -v base
test -v snapshot_dir
bash ./p373r-vrmod-devtime/experiments/fs/setup.bash
test -s fs-devtime/env
bash fs-devtime/env ./p373r-vrmod-devtime/experiments/fs/winxtropia-custom/build.bash 
