#!/bin/bash
set -euo pipefail

# tpvm.sh - Deterministic Build Driver

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

fail() { echo "$@" >&2; exit 1; }

usage() {
    echo "Usage: $0 {init|mod|compile|link} [args...]"
    exit 1
}

CMD=${1:-}
shift || true

case "$CMD" in
    init)
        SNAPSHOT_ARG=${1:-}
        if [ -z "$SNAPSHOT_ARG" ]; then
             # Try to find one in repo root _snapshot
             SNAPSHOT_ARG=$(find "$REPO_ROOT/_snapshot" -maxdepth 1 \( -type d -o -xtype d \) -name "fs-*" | head -n 1)
             test -n "$SNAPSHOT_ARG" || fail "Usage: $0 init <snapshot_dir> (and no fs-* snapshot found in _snapshot)"
        fi

        # Resolve absolute path of snapshot_dir if relative
        if [[ "$SNAPSHOT_ARG" != /* ]]; then
            # Check if it is relative to CWD
            if [ -d "$SNAPSHOT_ARG" ]; then
                SNAPSHOT_ARG="$(cd "$SNAPSHOT_ARG" && pwd)"
            elif [ -d "$REPO_ROOT/$SNAPSHOT_ARG" ]; then
                SNAPSHOT_ARG="$(cd "$REPO_ROOT/$SNAPSHOT_ARG" && pwd)"
            else
                fail "Snapshot directory not found: $SNAPSHOT_ARG"
            fi
        fi

        test -d "$SNAPSHOT_ARG" || fail "Snapshot directory not found: $SNAPSHOT_ARG"

        echo "Initializing with snapshot: $SNAPSHOT_ARG"

        # Verify llvm
        LLVM_DIR="$REPO_ROOT/llvm"
        test -d "$LLVM_DIR" || fail "LLVM toolchain not found at $LLVM_DIR"

        # Symlinks in CWD
        ln -sfn "$LLVM_DIR" llvm
        ln -sfn "$REPO_ROOT/winsdk" winsdk
        ln -sfn "$SNAPSHOT_ARG" snapshot

        mkdir -p env

        # Determine base
        SNAPSHOT_NAME=$(basename "$SNAPSHOT_ARG")
        # Assuming name format like fs-7.2.2-avx2 or similar
        BASE=$SNAPSHOT_NAME

        # Set variables for envsubst
        # We use relative paths for variables where possible/expected by rsp
        export snapshot_dir="snapshot"
        export winsdk="winsdk"
        export devtime="env"
        export base="$BASE"

        # Find _llvm include path
        # Assuming llvm is symlinked in CWD now
        _LLVM_INC=$(find llvm/lib/clang -path '*/include' -type d | head -n 1)
        if [ -z "$_LLVM_INC" ]; then
             fail "Could not find llvm include directory"
        fi
        export _llvm="$_LLVM_INC"

        TEMPLATES_DIR="$REPO_ROOT/experiments/fs/devtime.in"

        echo "Generating environment files in env/..."

        # Helper to subst
        subst() {
            envsubst '$base $snapshot_dir $devtime $winsdk $_llvm' < "$1" > "$2"
        }

        subst "snapshot/llobjs.rsp.in" "env/llobjs.rsp"
        subst <(sed -e 's@^-I@-isystem@g' "snapshot/llincludes.rsp.in") "env/llincludes.rsp"

        subst "$TEMPLATES_DIR/compile.common.rsp" "env/compile.common.rsp"
        subst "$TEMPLATES_DIR/link.common.rsp" "env/link.common.rsp"
        subst "$TEMPLATES_DIR/application-bin.rsp" "env/application-bin.rsp"

        if [[ "$BASE" == *fs-* ]]; then
            subst "$TEMPLATES_DIR/compile.fs.rsp" "env/compile.rsp"
            subst "$TEMPLATES_DIR/link.fs.rsp" "env/link.rsp"
        elif [[ "$BASE" == *sl-* ]]; then
            subst "$TEMPLATES_DIR/compile.sl.rsp" "env/compile.rsp"
            subst "$TEMPLATES_DIR/link.sl.rsp" "env/link.rsp"
        else
            echo "Warning: Defaulting to fs configuration."
            subst "$TEMPLATES_DIR/compile.fs.rsp" "env/compile.rsp"
            subst "$TEMPLATES_DIR/link.fs.rsp" "env/link.rsp"
        fi

        echo "$BASE" > env/base_name
        echo "Init complete."
        ;;

    mod)
        FILE_PART=${1:-}
        if [ -z "$FILE_PART" ]; then
            fail "Usage: $0 mod <partial_filename>"
        fi

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        # Search in snapshot/source
        FOUND=$(find snapshot/source -name "$FILE_PART" | head -n 1)

        if [ -z "$FOUND" ]; then
            fail "File not found in snapshot/source: $FILE_PART"
        fi

        cp "$FOUND" .
        echo "Copied $FOUND to $(pwd)/$(basename "$FOUND")"
        ;;

    compile)
        FILES=("$@")
        if [ ${#FILES[@]} -eq 0 ]; then
            FILES=(*.cpp)
        fi

        # Check if any files exist
        if [ ! -e "${FILES[0]}" ]; then
             echo "No files to compile."
             exit 0
        fi

        if [ ! -d "env" ] || [ ! -f "env/compile.rsp" ]; then
             fail "Environment not initialized. Run '$0 init' first."
        fi

        EXTRA_INCS="-I. -isystemsnapshot/source"
        if [ -d "$REPO_ROOT/sgeo-minimal" ]; then
            # sgeo-minimal is required for certain patched files (e.g. llviewerdisplay.cpp)
            EXTRA_INCS="$EXTRA_INCS -I$REPO_ROOT/sgeo-minimal"
        fi

        # Add mm.rsp which provides SSE intrinsics support.
        # This is explicitly injected by build.bash, so we replicate that here.
        # Without this, compilation of graphics code (like llviewerdisplay.cpp) fails.
        MM_RSP="@winsdk/mm.rsp"

        for FILE in "${FILES[@]}"; do
            if [ -f "$FILE" ]; then
                OBJ="${FILE}.obj"
                echo "Compiling $FILE..." >&2
                echo "./llvm/bin/clang++ @env/compile.rsp $MM_RSP $EXTRA_INCS -c $FILE ${CXXFLAGS:-} -o $OBJ"

                ./llvm/bin/clang++ @"env/compile.rsp" $MM_RSP $EXTRA_INCS -c "$FILE" ${CXXFLAGS:-}  -o "$OBJ"
            fi
        done
        ;;

    link)
        if [ ! -d "env" ] || [ ! -f "env/link.rsp" ]; then
             fail "Environment not initialized. Run '$0 init' first."
        fi

        LOCAL_OBJS=(*.obj)
        if [ ! -e "${LOCAL_OBJS[0]}" ]; then
            LOCAL_OBJS=()
        fi

        echo "Filtering object files... ${LOCAL_OBJS[0]}" >&2

        # Python script to filter llobjs.rsp
        cat <<EOF > env/filter_llobjs.py
import sys
import os

local_objs = sys.argv[1:]
local_obj_names = {os.path.basename(f) for f in local_objs}

try:
    with open('env/llobjs.rsp', 'r') as f:
        lines = f.readlines()

    filtered_lines = []
    removed_count = 0
    for line in lines:
        line = line.strip()
        if not line: continue
        fname = os.path.basename(line)
        if fname in local_obj_names:
            removed_count += 1
            continue
        filtered_lines.append(line)

    with open('env/llobjs_filtered.rsp', 'w') as f:
        f.write('\n'.join(filtered_lines))

    print(f"Filtered {removed_count} overridden objects.")

except Exception as e:
    print(f"Error filtering objects: {e}")
    sys.exit(1)
EOF

        python3 env/filter_llobjs.py "${LOCAL_OBJS[@]}"

        if [ -f "env/base_name" ]; then
            BASE_NAME=$(cat env/base_name)
        else
            BASE_NAME="tpvm"
        fi
        OUTPUT_EXE="${BASE_NAME}.exe"

        echo "Linking $OUTPUT_EXE..." >&2
        echo "./llvm/bin/clang++ @env/link.rsp @env/llobjs_filtered.rsp ${LOCAL_OBJS[@]} ${LDFLAGS:-} -o $OUTPUT_EXE"

        # Note: We use clang++ driver for linking instead of lld-link directly.
        # This is because the .rsp files (link.common.rsp, winsdk.rsp) contain
        # Clang driver flags (e.g., -Wl, -target, -isystem) which are not supported
        # by lld-link. clang++ will invoke lld-link (via -fuse-ld=lld) with correct arguments.
        # This matches the behavior of build.bash ("known toolchain overall invocation patterns").
        ./llvm/bin/clang++ @"env/link.rsp" @"env/llobjs_filtered.rsp" "${LOCAL_OBJS[@]}" ${LDFLAGS:-} -o "$OUTPUT_EXE"

        echo "Link complete: $OUTPUT_EXE" >&2
        ;;

    *)
        usage
        ;;
esac
