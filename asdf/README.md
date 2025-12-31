# these might already exist but to make sure run:
ln -sTf /usr/lib/llvm-19 /opt/humbletim/llvm
ln -sTf /opt/humbletim/llvm /app/llvm

# all these commands are meant to run from this asdf folder
# SO from the repo root you would invoke as `cd asdf && ../experiments/tpvm.sh ...`

# Initialize the environment (generates shim, wchar.rsp, etc.)
../experiments/tpvm.sh init ../_snapshot/fs-7.2.2-avx2

# Mod files (copies from snapshot)
../experiments/tpvm.sh mod llstartup.cpp
../experiments/tpvm.sh mod fsdata.cpp

# Patch files
patch -p3 < fsdata.cpp.patch
patch -p3 < llstartup.cpp.patch

# Compile files
# Note: tpvm.sh now automatically handles wchar_t flags and shims.
# fsdata.cpp no longer needs polyfills.
# llstartup.cpp still needs an extra include path (for now).
CXXFLAGS="-isystemsnapshot/source/viewer_components/login" ../experiments/tpvm.sh compile llstartup.cpp
../experiments/tpvm.sh compile fsdata.cpp

# Link
../experiments/tpvm.sh link

# diff -u snapshot/source/newview/ llstartup.cpp > llstartup.cpp.patch
# diff -u snapshot/source/newview/ fsdata.cpp > fsdata.cpp.patch

## Investigation Report: `wchar_t` / `tsoob.c++` Elimination

### Goal
Eliminate the need for the `tsoob.c++` shim, which works around type mismatches between Clang's `wchar_t` and the MSVC-compiled SDK/codebase. The goal was to find a "native" compiler configuration that allows Clang to compile the code as MSVC does (treating `wchar_t` as `unsigned short`).

### Findings

1.  **The Mismatch:**
    *   Clang defaults to treating `wchar_t` as a built-in distinct type.
    *   The Windows SDK and the Viewer codebase (via `llstring.h`) expect `wchar_t` to be a typedef for `unsigned short` (when `/Zc:wchar_t-` is used, which is the legacy behavior this codebase relies on).
    *   This caused `llutf16string` (defined as `std::basic_string<unsigned short>`) to be incompatible with string literals `L"..."` (which Clang saw as `const wchar_t*`).

2.  **Failed Attempts:**
    *   `-fshort-wchar`: Makes `wchar_t` 16-bit, but it remains a distinct type. Mismatch persists.
    *   `-Dwchar_t="unsigned short"`: Macroizing `wchar_t` without other changes failed because standard headers (specifically Windows SDK CRT headers like `xkeycheck.h`) forbid macroizing keywords and throw fatal errors.
    *   Bypassing `xkeycheck.h` (via `_XKEYCHECK_H` define): Caused massive breakage in STL and Boost due to conflicting template specializations and literal operators.

3.  **The Working Solution:**
    *   **Flag:** `-Xclang -fno-wchar`. This tells the Clang frontend to **stop treating `wchar_t` as a keyword**.
    *   **SDK Behavior:** When `wchar_t` is not a keyword, the Windows SDK header `corecrt.h` detects that `_WCHAR_T_DEFINED` is unset, and executes `typedef unsigned short wchar_t;`. This effectively aligns the types.
    *   **Header Override (`llpreprocessor.h`):**
        *   We must prevent `llpreprocessor.h` from forcing `LL_WCHAR_T_NATIVE 1` when `__clang__` is defined.
        *   We must define `BOOST_NO_INTRINSIC_WCHAR_T` to prevent Boost from specializing templates for `wchar_t` (which is now just `unsigned short`, causing "redefinition" errors if Boost tries to handle both).

### Final Working Configuration

To compile `fsdata.cpp` (or other affected files) without `tsoob.c++`:

1.  **Use the modified `llpreprocessor.h`** (found in `asdf/llpreprocessor.h`).
2.  **Pass Compiler Flags:**
    ```bash
    -Xclang -fno-wchar -fms-extensions -include llpreprocessor.h
    ```
3.  **Ensure `tsoob.c++` is removed** from the compilation unit (e.g., filter it out of `.rsp` files).

### Reproducing Success
The file `wchar_compile_test.cpp` serves as the verification harness. It compiles successfully with the above configuration, proving that `llutf16string` (std::basic_string<unsigned short>) can be constructed from `L"..."` string literals.

```bash
# Example invocation
cd asdf
CXXFLAGS="-include llpreprocessor.h -Xclang -fno-wchar -fms-extensions" ../experiments/tpvm.sh compile wchar_compile_test.cpp
```

### Applying `llpreprocessor.h` Patch
Instead of using a full copy of the header, apply the patch:
```bash
cp snapshot/source/llcommon/llpreprocessor.h llpreprocessor.h
patch -p1 < llpreprocessor.h.patch
```
