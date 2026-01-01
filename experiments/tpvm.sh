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
        ARG1=${1:-}

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        check_file_info() {
            local f=$1
            local snap_path=$2

            local status_diff="N/A"
            local status_poly=""
            local status_obj=""
            local is_override=0

            if [ -n "$snap_path" ] && [ -f "$snap_path" ]; then
                is_override=1
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

            echo "$is_override|$f|$status_diff|$status_poly|$status_obj"
        }

        # Gather all files
        # Use find to avoid errors if no files match
        ALL_FILES=()
        while IFS= read -r f; do
             if [ -f "$f" ]; then
                 ALL_FILES+=("$(basename "$f")")
             fi
        done < <(find . -maxdepth 1 \( -name "*.cpp" -o -name "*.h" \) -type f)

        OVERRIDES=()
        LOCAL_ONLY=()

        for f in "${ALL_FILES[@]}"; do
             # Find in snapshot
             FOUND=$(find snapshot/source -name "$f" 2>/dev/null | head -n 1)
             INFO=$(check_file_info "$f" "$FOUND")
             IS_OVERRIDE=$(echo "$INFO" | cut -d'|' -f1)

             if [ "$IS_OVERRIDE" -eq 1 ]; then
                 OVERRIDES+=("$INFO")
             else
                 LOCAL_ONLY+=("$INFO")
             fi
        done

        # Sort lists (by filename, which is field 2)
        IFS=$'\n' OVERRIDES=($(sort -t'|' -k2 <<<"${OVERRIDES[*]}"))
        IFS=$'\n' LOCAL_ONLY=($(sort -t'|' -k2 <<<"${LOCAL_ONLY[*]}"))
        unset IFS

        if [[ "$ARG1" == "overrides" ]]; then
            # Output only filenames of overridden files
            for info in "${OVERRIDES[@]}"; do
                echo "$info" | cut -d'|' -f2
            done
        else
            # Output full status table
             printf "%-30s %-15s %-15s %-15s\n" "File" "Snapshot Diff" "Polyfills" "Object"
             echo "-------------------------------------------------------------------------------"

             for info in "${OVERRIDES[@]}"; do
                 F_NAME=$(echo "$info" | cut -d'|' -f2)
                 S_DIFF=$(echo "$info" | cut -d'|' -f3)
                 S_POLY=$(echo "$info" | cut -d'|' -f4)
                 S_OBJ=$(echo "$info" | cut -d'|' -f5)
                 printf "%-30s %-15s %-15s %-15s\n" "$F_NAME" "$S_DIFF" "$S_POLY" "$S_OBJ"
             done

             for info in "${LOCAL_ONLY[@]}"; do
                 F_NAME=$(echo "$info" | cut -d'|' -f2)
                 S_DIFF=$(echo "$info" | cut -d'|' -f3)
                 S_POLY=$(echo "$info" | cut -d'|' -f4)
                 S_OBJ=$(echo "$info" | cut -d'|' -f5)
                 printf "%-30s %-15s %-15s %-15s\n" "$F_NAME" "$S_DIFF" "$S_POLY" "$S_OBJ"
             done
        fi
        ;;

    mergetool)
        BASENAME=${1:-}
        if [ -z "$BASENAME" ]; then
            fail "Usage: $0 mergetool <basename>"
        fi

        # Find local file
        LOCAL_FILE=""
        if [ -f "$BASENAME" ]; then
            LOCAL_FILE="$BASENAME"
        elif [ -f "${BASENAME}.cpp" ]; then
            LOCAL_FILE="${BASENAME}.cpp"
        elif [ -f "${BASENAME}.h" ]; then
            LOCAL_FILE="${BASENAME}.h"
        else
             fail "Local file not found for basename: $BASENAME"
        fi

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        FOUND=$(find snapshot/source -name "$LOCAL_FILE" | head -n 1)
        if [ -z "$FOUND" ]; then
            fail "File not found in snapshot/source: $LOCAL_FILE"
        fi

        echo "Running meld on $FOUND and $LOCAL_FILE..."
        meld "$FOUND" "$LOCAL_FILE"
        ;;

    patch)
        BASENAME=${1:-}
        if [ -z "$BASENAME" ]; then
            fail "Usage: $0 patch <basename>"
        fi

        PATCH_FILE="${BASENAME}.patch"
        if [ ! -f "$PATCH_FILE" ]; then
             fail "Patch file not found: $PATCH_FILE"
        fi

        # Determine target filename from patch file content or just try basename?
        # User said "attempt to apply a patch named basename.patch from the current folder to the local copy"
        # The local copy might be basename.cpp or basename.h?
        # Typically the patch file implies the file. But we need to ensure the local file exists.
        # Let's assume the user means the file that corresponds to the basename.
        # If basename is "fsdata.cpp", then file is fsdata.cpp.
        # If basename is "fsdata", we might need to guess.
        # Let's check if basename looks like a file.

        TARGET_FILE="$BASENAME"
        # If basename doesn't have extension but patch does?
        # User example: patch -p3 < fsdata.cpp.patch. Basename seems to be "fsdata.cpp" in the command likely.

        # Check if local file exists
        CREATED_BY_CMD=0
        if [ ! -f "$TARGET_FILE" ]; then
             echo "Local file $TARGET_FILE does not exist. Attempting mod..."
             # Call mod logic (function or recursive)
             # Reuse mod logic by calling self
             "$0" mod "$TARGET_FILE"
             CREATED_BY_CMD=1
        fi

        if [ ! -d "snapshot" ]; then
            fail "Snapshot symlink not found. Run '$0 init' first."
        fi

        FOUND=$(find snapshot/source -name "$TARGET_FILE" | head -n 1)
        if [ -z "$FOUND" ]; then
             fail "File not found in snapshot/source: $TARGET_FILE"
        fi

        # Verify unmodified from snapshot
        if ! cmp -s "$FOUND" "$TARGET_FILE"; then
             fail "Local file $TARGET_FILE is modified compared to snapshot. Aborting patch."
        fi

        echo "Applying patch $PATCH_FILE to $TARGET_FILE..."
        # Try applying patch.
        # User manual uses -p3. I should probably be flexible or just try.
        # But wait, patch command usually reads from stdin or file.
        # I'll try `patch -i PATCH_FILE` which automatically detects headers usually.
        # Or `patch < PATCH_FILE`.
        # I'll rely on `patch` to figure it out or use common flags?
        # The user's manual has `patch -p3 < ...`
        # If I run just `patch`, it might ask for file.
        # I'll provide the file as argument to patch if possible. `patch TARGET_FILE < PATCH_FILE`?
        # Or `patch -i PATCH_FILE TARGET_FILE`?
        # Standard patch: `patch [options] [originalfile [patchfile]]`

        if patch -i "$PATCH_FILE" "$TARGET_FILE"; then
             echo "Patch applied successfully."
        else
             echo "Patch failed."
             if [ "$CREATED_BY_CMD" -eq 1 ]; then
                 echo "Removing created file $TARGET_FILE..."
                 rm "$TARGET_FILE"
             else
                 echo "Restoring $TARGET_FILE from snapshot..."
                 cp "$FOUND" "$TARGET_FILE"
             fi
             exit 1
        fi
        ;;

    compile_)
        BASENAME=${1:-}
        if [ -z "$BASENAME" ]; then
            fail "Usage: $0 compile_ <basename>"
        fi

        SRC_FILE=""
        if [ -f "$BASENAME" ]; then
            SRC_FILE="$BASENAME"
        elif [ -f "${BASENAME}.cpp" ]; then
            SRC_FILE="${BASENAME}.cpp"
        else
             fail "Source file not found: $BASENAME"
        fi

        OBJ_FILE="${SRC_FILE}.obj"

        NEEDS_COMPILE=0

        if [ ! -f "$OBJ_FILE" ]; then
            NEEDS_COMPILE=1
            REASON="Object file missing"
        else
            # Mtime check of source
            if [ "$SRC_FILE" -nt "$OBJ_FILE" ]; then
                NEEDS_COMPILE=1
                REASON="$SRC_FILE newer than object"
            fi
        fi

        DEPS=("$SRC_FILE")

        # Scan immediate headers
        # Naive: grep #include "..."
        # We only care about header files that we can find locally or in snapshot/source
        # to check their mtime.

        # Get list of included files (naive regex)
        INCLUDES=$(grep -E '^\s*#\s*include\s*"' "$SRC_FILE" | cut -d'"' -f2)

        for inc in $INCLUDES; do
            # Resolve
            RESOLVED=""
            if [ -f "$inc" ]; then
                RESOLVED="$inc"
            elif [ -d "snapshot" ]; then
                 # Try finding in snapshot/source
                 # This might be slow if we search everything.
                 # User said "immediate headers".
                 # He also said "if any of them are newer...".
                 # If header is not in local dir, check if it's in snapshot.
                 # But headers in snapshot might be in subdirs.
                 # We can use `find snapshot/source -name basename`
                 FOUND=$(find snapshot/source -name "$(basename "$inc")" -print -quit)
                 if [ -n "$FOUND" ]; then
                     RESOLVED="$FOUND"
                 fi
            fi

            if [ -n "$RESOLVED" ]; then
                DEPS+=("$(basename "$RESOLVED")")
                if [ "$NEEDS_COMPILE" -eq 0 ] && [ "$RESOLVED" -nt "$OBJ_FILE" ]; then
                    NEEDS_COMPILE=1
                    REASON="$RESOLVED newer than object"
                fi
            fi
        done

        if [ "$NEEDS_COMPILE" -eq 1 ]; then
             echo "Compiling due to: $REASON"
             "$0" compile "$SRC_FILE"
        else
             echo "No modifications detected."
             echo "Scanned dependencies: ${DEPS[*]}"
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
