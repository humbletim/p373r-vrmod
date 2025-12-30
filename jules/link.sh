#!/bin/bash
set -euo pipefail

# Usage: ./jules/link.sh

OUTPUT_EXE="jules/build/winxtropia-custom.exe"
LOBJS_RSP="jules/build/llobjs.custom.rsp"
APP_RSP="jules/build/application-bin.custom.rsp"

echo "Preparing link..."

# 1. Create a custom llobjs.rsp based on the original
cp fs-devtime/llobjs.rsp "$LOBJS_RSP"

# 2. Iterate over our compiled objects and substitute them in the RSP
for OBJ in jules/build/*.obj; do
    [ -e "$OBJ" ] || continue

    BASENAME=$(basename "$OBJ")
    # indra_newview_llstartup.cpp.obj -> newview/llstartup.cpp.obj
    SEARCH_PATTERN="${BASENAME#indra_}"
    SEARCH_PATTERN="${SEARCH_PATTERN//_//}"

    echo "Substituting object for $SEARCH_PATTERN with $OBJ"

    if grep -q "$SEARCH_PATTERN" "$LOBJS_RSP"; then
         ESCAPED_OBJ=$(echo "$OBJ" | sed 's/\//\\\//g')
         sed -i "s|.*$SEARCH_PATTERN.*|$OBJ|" "$LOBJS_RSP"
    else
        echo "Warning: Could not find entry for $SEARCH_PATTERN in llobjs.rsp"
    fi
done

# 3. Create application-bin.custom.rsp
echo "@fs-devtime/link.rsp @$LOBJS_RSP" > "$APP_RSP"

echo "Linking to $OUTPUT_EXE..."

LLVM_BIN="llvm/bin/clang++"
if [ ! -x "$LLVM_BIN" ]; then
    LLVM_BIN="clang++"
fi

$LLVM_BIN @"$APP_RSP" -o "$OUTPUT_EXE"

echo "Link successful: $OUTPUT_EXE"
