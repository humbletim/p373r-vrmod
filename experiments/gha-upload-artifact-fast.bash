#!/bin/bash

# wrapper to invoke actions/upload-artifact from within an actions/tmate debugging ssh session 
# - humbletim 2025.11.07

gha-esc() {( printf "%q" "$@" )}
gha-upload-artifact-fast() {(
    local name=$1 path=$2 expires=${3:-1} compress=${4:-0} overwrite=${5:-false}
    # set -x;
    test -v GITHUB_ACTIONS || return 0;
    set -Euo pipefail;
    export INPUT_NAME="`gha-esc "$name"`";
    export INPUT_PATH="`gha-esc "$path"`";
    local actions=
    case "$OSTYPE" in
      msys|cygwin) actions=/d/_a/_actions/actions  ;;
      *)           actions=~/work/_actions/actions ;;
    esac
    test -n "$actions" || { echo "OSTYPE=$OSTYPE ???" >&2 ; return 60 ; } 
    env INPUT_RETENTION-DAYS=${expires} INPUT_COMPRESSION-LEVEL=${compress} INPUT_OVERWRITE=${overwrite} INPUT_INCLUDE-HIDDEN-FILES=true INPUT_IF-NO-FILES-FOUND=error \
        node ${actions}/upload-artifact/v4/dist/upload/index.js || return $?;
    echo "uploaded: ${INPUT_PATH}" 1>&2 )
}

# gha-upload-artifact-fast "$@"
