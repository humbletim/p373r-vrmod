#!/bin/bash
set -euo pipefail

# Determine Repo Root
SCRIPT_PATH=$(readlink -f "${BASH_SOURCE[0]}")
REPO_ROOT=$(cd "$(dirname "$SCRIPT_PATH")/.." && pwd)

# Helper: fail
fail() { echo "$@" >&2; exit 1; }

usage() {
    echo "Usage: $0 {init|mod|compile|link} [args...]"
    exit 1
}

CMD=${1:-}
shift || true
if [ -z "$CMD" ]; then usage; fi

case "$CMD" in
    init)
        SNAPSHOT_ARG=${1:-}
        if [ -z "$SNAPSHOT_ARG" ]; then
            fail "Usage: $0 init <snapshot_dir>"
        fi

        # Resolve absolute path of snapshot arg
        ABS_SNAPSHOT_ARG=$(readlink -f "$SNAPSHOT_ARG")
        if [ ! -d "$ABS_SNAPSHOT_ARG" ]; then
            fail "Snapshot directory not found: $ABS_SNAPSHOT_ARG"
        fi

        # Derive base
        BASE=$(basename "$ABS_SNAPSHOT_ARG")

        # Verify LLVM
        if [ ! -d "$REPO_ROOT/llvm" ]; then
            fail "LLVM toolchain not found at $REPO_ROOT/llvm"
        fi

        # Create CWD/snapshot
        ln -snf "$ABS_SNAPSHOT_ARG" ./snapshot

        # Create CWD/env
        mkdir -p ./env

        # Symlink winsdk to CWD
        if [ -d "$REPO_ROOT/winsdk" ]; then
             ln -snf "$REPO_ROOT/winsdk" ./winsdk
        else
             fail "winsdk directory not found at $REPO_ROOT/winsdk"
        fi

        # Prepare variables for envsubst
        export base="$BASE"
        export snapshot_dir="$(readlink -f ./snapshot)"
        export devtime="$(readlink -f ./env)"
        export winsdk="$(readlink -f ./winsdk)"

        # Find _llvm include path
        _LLVM_PATH=$(find "$REPO_ROOT/llvm/lib/clang" -path '*/include' -type d | head -n 1)
        if [ -z "$_LLVM_PATH" ]; then
            fail "Could not find llvm includes in $REPO_ROOT/llvm"
        fi
        export _llvm="$_LLVM_PATH"

        echo "Initializing environment in ./env for base=$base..."

        safeenvsubst() { envsubst '$base $snapshot_dir $devtime $winsdk $_llvm'; }

        # Process templates
        if [ -f "$snapshot_dir/llobjs.rsp.in" ]; then
            safeenvsubst < "$snapshot_dir/llobjs.rsp.in" > "./env/llobjs.rsp"
        fi
        if [ -f "$snapshot_dir/llincludes.rsp.in" ]; then
            safeenvsubst < "$snapshot_dir/llincludes.rsp.in" > "./env/llincludes.rsp"
        fi

        TPL_DIR="$REPO_ROOT/experiments/fs/devtime.in"

        if [ -f "$TPL_DIR/compile.common.rsp" ]; then
            safeenvsubst < "$TPL_DIR/compile.common.rsp" > "./env/compile.common.rsp"
        fi
        if [ -f "$TPL_DIR/link.common.rsp" ]; then
            safeenvsubst < "$TPL_DIR/link.common.rsp" > "./env/link.common.rsp"
        fi
        if [ -f "$TPL_DIR/application-bin.rsp" ]; then
            safeenvsubst < "$TPL_DIR/application-bin.rsp" > "./env/application-bin.rsp"
        fi
        if [ -f "$TPL_DIR/base.env.in" ]; then
            safeenvsubst < "$TPL_DIR/base.env.in" > "./env/env"
        fi

        if [[ "$base" == *fs-* ]]; then
            safeenvsubst < "$TPL_DIR/compile.fs.rsp" > "./env/compile.rsp"
            safeenvsubst < "$TPL_DIR/link.fs.rsp" > "./env/link.rsp"
        elif [[ "$base" == *sl-* ]]; then
            safeenvsubst < "$TPL_DIR/compile.sl.rsp" > "./env/compile.rsp"
            safeenvsubst < "$TPL_DIR/link.sl.rsp" > "./env/link.rsp"
        fi

        echo "Environment initialized."
        ;;

    mod)
        PARTIAL="$1"
        if [ -z "$PARTIAL" ]; then
            fail "Usage: $0 mod <partial_filename>"
        fi

        echo "Searching for *$PARTIAL* in ./snapshot/source..."
        FOUND=$(find -L ./snapshot/source -name "*$PARTIAL*" -type f -print -quit)

        if [ -z "$FOUND" ]; then
            fail "File not found."
        fi

        echo "Found: $FOUND"
        cp -v "$FOUND" .
        ;;

    compile)
        FILES=("$@")
        if [ ${#FILES[@]} -eq 0 ]; then
            shopt -s nullglob
            FILES=(*.cpp)
            shopt -u nullglob
        fi

        if [ ! -f ./env/compile.rsp ]; then
            fail "Environment not initialized. Run 'init' first."
        fi

        COMPILE_RSP="$(readlink -f ./env/compile.rsp)"
        CLANG="$REPO_ROOT/llvm/bin/clang++"
        # Using ./winsdk/_vfsoverlay.json which handles SDK file casing.
        # build.bash also adds @winsdk/mm.rsp and -I flags.
        # However, compile.rsp (generated from compile.fs.rsp) includes compile.common.rsp which includes winsdk.rsp.
        # winsdk.rsp includes -ivfsoverlay winsdk/_vfsoverlay.json.
        # So explicit -vfsoverlay might be redundant if winsdk.rsp is used, but build.bash adds it explicitly.
        # "Invokes the internal Clang command using the exact arguments from the snapshot"
        # The snapshot likely uses devtime/compile.rsp.
        # But wait, build.bash adds: @winsdk/mm.rsp -vfsoverlay build/vfsoverlay.yml -Ibuild/ -I"$vrmod_dir"
        # The prompt said: "exact arguments from the snapshot: eg: clang++ @.../compile.rsp -c local_file.cpp -o local_file.obj"
        # I will trust the prompt and compile.rsp content, but add the SDK VFS overlay just in case it's not fully covered or for consistency.

        for FILE in "${FILES[@]}"; do
            [ -f "$FILE" ] || continue
            OBJ="${FILE}.obj"
            echo "Compiling $FILE -> $OBJ"
            "$CLANG" @"$COMPILE_RSP" -c "$FILE" -o "$OBJ"
        done
        ;;

    link)
        if [ ! -f ./env/llobjs.rsp ]; then
            fail "Environment not initialized."
        fi

        # Identify local objects
        shopt -s nullglob
        LOCAL_OBJS=(*.obj)
        shopt -u nullglob

        if [ ${#LOCAL_OBJS[@]} -eq 0 ]; then
            echo "No local objects found."
        fi

        echo "Filtering llobjs.rsp..."

        > exclusion_list.txt
        for obj in "${LOCAL_OBJS[@]}"; do
             esc_obj=$(echo "$obj" | sed 's/[.[\*^$]/\\&/g')
             echo "[/\\\\]$esc_obj\$" >> exclusion_list.txt
        done

        if [ -s exclusion_list.txt ]; then
            grep -v -f exclusion_list.txt ./env/llobjs.rsp > temp_filtered_llobjs.rsp
        else
            cp ./env/llobjs.rsp temp_filtered_llobjs.rsp
        fi

        LINK_RSP="$(readlink -f ./env/link.rsp)"
        CLANG="$REPO_ROOT/llvm/bin/clang++"
        OUTPUT_EXE="output.exe"
        VFS_OVERLAY="./winsdk/_vfsoverlay.json"

        echo "Linking to $OUTPUT_EXE..."

        # Using clang++ to drive the link, passing the SDK VFS overlay.
        # This matches the pattern in build.bash (except we don't have the custom build/vfsoverlay.yml for other things, just the SDK one).
        "$CLANG" @"$LINK_RSP" \
                 -vfsoverlay "$VFS_OVERLAY" \
                 -Wl,/vfsoverlay:"$VFS_OVERLAY" \
                 @temp_filtered_llobjs.rsp \
                 "${LOCAL_OBJS[@]}" \
                 -o "$OUTPUT_EXE"
        ;;

    *)
        usage
        ;;
esac
