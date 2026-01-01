#!/bin/bash
set -euo pipefail

# tpvm.sh - Deterministic Build Driver

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

fail() { echo "$@" >&2; exit 1; }

usage() {
    echo "tpvm.sh - Deterministic Build Driver"
    echo ""
    echo "Usage: $0 <command> [arguments]"
    echo ""
    echo "Commands:"
    echo "  init <snapshot_dir>   Initialize environment with snapshot."
    echo "  mod <filename>        Copy file from snapshot to local directory."
    echo "  diff <filename>       Diff local file against snapshot."
    echo "  status [filename]     Show status of local files vs snapshot and polyfills."
    echo "  compile [files...]    Compile specific .cpp files (defaults to *.cpp)."
    echo "  link                  Link the application using local objects."
    echo ""
    echo "Examples:"
    echo "  $0 init _snapshot/fs-7.2.2-avx2"
    echo "  $0 mod llstartup.cpp"
    echo "  $0 status"
    echo "  $0 compile llstartup.cpp"
    echo "  $0 link"
    exit 1
}

if [[ "${1:-}" == "--help" ]] || [[ -z "${1:-}" ]]; then
    usage
fi

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
        ln -sfn "$SNAPSHOT_ARG" snapshot

        mkdir -p env

        # Local winsdk setup to hold generated rsp files while symlinking heavy artifacts
        mkdir -p winsdk
        ln -sfn "$REPO_ROOT/winsdk/crt" winsdk/crt
        ln -sfn "$REPO_ROOT/winsdk/sdk" winsdk/sdk

        # Merge vfsoverlay
        jq -s ".[0] * { roots: (.[0].roots + .[1].roots) }" \
            "$REPO_ROOT/experiments/winsdk.in/vfsoverlay.extra.json" \
            "$REPO_ROOT/winsdk/vfsoverlay.json" > winsdk/_vfsoverlay.json

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

        # Generate winsdk rsp files
        subst "$REPO_ROOT/experiments/winsdk.in/winsdk.rsp" "winsdk/winsdk.rsp"
        subst "$REPO_ROOT/experiments/winsdk.in/mm.rsp" "winsdk/mm.rsp"

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

        # Create llpreprocessor_shim.h in env
        cat <<EOF > env/llpreprocessor_shim.h
#ifdef __clang__
#undef __clang__
#define __RESTORE_CLANG__
#endif

// Prevent Boost from specializing for wchar_t since it's now unsigned short
#define BOOST_NO_INTRINSIC_WCHAR_T 1

#include <llpreprocessor.h>

#ifdef __RESTORE_CLANG__
#define __clang__ 1
#undef __RESTORE_CLANG__
#endif
EOF
        # Generate wchar.rsp
        # -Xclang -fno-wchar: Treat wchar_t as unsigned short (MSVC compat)
        # -fms-extensions: Enable MS extensions
        # -include env/llpreprocessor_shim.h: Sandwich llpreprocessor.h to hide __clang__ during its inclusion
        (
            echo ""
            echo "-Xclang -fno-wchar -fms-extensions -includeenv/llpreprocessor_shim.h"
            echo ""
            EXTRA_INCS="-I. -isystemsnapshot/source"
            if [ -d "$REPO_ROOT/sgeo-minimal" ]; then
                # sgeo-minimal is required for certain patched files (e.g. llviewerdisplay.cpp)
                EXTRA_INCS="$EXTRA_INCS -I$REPO_ROOT/sgeo-minimal"
            fi
            echo "$EXTRA_INCS"
        ) >> env/compile.rsp

        echo "Created env/llpreprocessor_shim.h shim and appended env/compile.rsp to utilize."

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

    diff)
        FILE_PART=${1:-}
        if [ -z "$FILE_PART" ]; then
            fail "Usage: $0 diff <partial_filename>"
        fi

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        # Search in snapshot/source
        FOUND=$(find snapshot/source -name "$FILE_PART" | head -n 1)

        if [ -z "$FOUND" ]; then
            fail "File not found in snapshot/source: $FILE_PART"
        fi

        LOCAL_FILE=$(basename "$FOUND")
        if [ ! -f "$LOCAL_FILE" ]; then
            fail "Local file not found: $LOCAL_FILE"
        fi

        diff -u "$FOUND" "$LOCAL_FILE" || true
        ;;

    status)
        FILE_PART=${1:-}

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        # Header
        printf "%-30s %-15s %-15s %-15s\n" "File" "Snapshot Diff" "Polyfills" "Object"
        echo "-------------------------------------------------------------------------------"

        check_file() {
            local f=$1
            local snap_path=$2

            local status_diff="N/A"
            local status_poly=""
            local status_obj=""

            if [ -n "$snap_path" ] && [ -f "$snap_path" ]; then
                if cmp -s "$snap_path" "$f"; then
                    status_diff="Same"
                else
                    status_diff="Modified"
                fi
            else
                status_diff="Local Only"
            fi

            local base=$(basename "$f" .cpp)
            base=$(basename "$base" .h)

            if [ -f "polyfills/${base}.compile.rsp" ]; then
                status_poly="${status_poly}C"
            fi
            if [ -f "polyfills/${base}.link.rsp" ]; then
                status_poly="${status_poly}L"
            fi
            [ -z "$status_poly" ] && status_poly="-"

            # Check for object file (foo.cpp.obj)
            if [ -f "${f}.obj" ]; then
                status_obj="Present"
            else
                status_obj="-"
            fi

            printf "%-30s %-15s %-15s %-15s\n" "$f" "$status_diff" "$status_poly" "$status_obj"
        }

        if [ -n "$FILE_PART" ]; then
             # Check specific file
             FOUND=$(find snapshot/source -name "$FILE_PART" 2>/dev/null | head -n 1)
             LOCAL_FILE=$(basename "$FILE_PART")
             if [ -f "$LOCAL_FILE" ]; then
                 check_file "$LOCAL_FILE" "$FOUND"
             else
                 # Try to find it if it was just a name like llstartup.cpp and it is in current dir
                 if [ -f "$FILE_PART" ]; then
                     FOUND=$(find snapshot/source -name "$FILE_PART" 2>/dev/null | head -n 1)
                     check_file "$FILE_PART" "$FOUND"
                 else
                     echo "File not found locally: $FILE_PART"
                 fi
             fi
        else
            # Scan all local cpp/h files
            # Use find to avoid errors if no files match
            find . -maxdepth 1 \( -name "*.cpp" -o -name "*.h" \) -type f | while read -r f; do
                f=$(basename "$f")
                FOUND=$(find snapshot/source -name "$f" 2>/dev/null | head -n 1)
                check_file "$f" "$FOUND"
            done
        fi
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

        echo "Compiling ${FILES[@]}" >&2
        for FILE in "${FILES[@]}"; do
            if [ -f "$FILE" ]; then
                OBJ="${FILE}.obj"
                POLYFILL=
                if [ -s "polyfills/$(basename -s .cpp $FILE).compile.rsp" ] ; then
                    POLYFILL="@polyfills/$(basename -s .cpp $FILE).compile.rsp"
                fi
                echo "Compiling $FILE... $POLYFILL" >&2
                echo "./llvm/bin/clang++ @env/compile.rsp $POLYFILL -c $FILE ${CXXFLAGS:-} -o $OBJ"

                ./llvm/bin/clang++ @"env/compile.rsp" $POLYFILL -c "$FILE" ${CXXFLAGS:-} -o "$OBJ"
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

        # Auto-detect polyfills for local objects
        POLYFILL_ARGS=()
        if [ ${#LOCAL_OBJS[@]} -gt 0 ]; then
            for OBJ in "${LOCAL_OBJS[@]}"; do
                # OBJ is like llstartup.cpp.obj
                # Basename logic: llstartup.cpp.obj -> llstartup.cpp -> llstartup
                BASE=$(basename "$OBJ" .obj)
                BASE=$(basename "$BASE" .cpp)

                LINK_RSP="polyfills/${BASE}.link.rsp"
                if [ -f "$LINK_RSP" ]; then
                    echo "Injecting linker polyfill: $LINK_RSP" >&2
                    POLYFILL_ARGS+=("@$LINK_RSP")
                fi
            done
        fi

        echo "Linking $OUTPUT_EXE..." >&2
        echo "./llvm/bin/clang++ @env/link.rsp @env/llobjs_filtered.rsp ${LOCAL_OBJS[@]} ${POLYFILL_ARGS[@]} ${LDFLAGS:-} -o $OUTPUT_EXE"

        # Note: We use clang++ driver for linking instead of lld-link directly.
        # This is because the .rsp files (link.common.rsp, winsdk.rsp) contain
        # Clang driver flags (e.g., -Wl, -target, -isystem) which are not supported
        # by lld-link. clang++ will invoke lld-link (via -fuse-ld=lld) with correct arguments.
        # This matches the behavior of build.bash ("known toolchain overall invocation patterns").
        ./llvm/bin/clang++ @"env/link.rsp" @"env/llobjs_filtered.rsp" "${LOCAL_OBJS[@]}" "${POLYFILL_ARGS[@]}" ${LDFLAGS:-} -o "$OUTPUT_EXE"

        echo "Link complete: $OUTPUT_EXE" >&2
        ;;

    *)
        usage
        ;;
esac
