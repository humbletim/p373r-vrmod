# Boost Signals ABI Mismatch Investigation

## The Problem
The Viewer crashes when logging in if `llviewerregion.h` is modified to use `boost::signals2::signal<void(..., LLViewerRegion* const&)>` instead of the original `LLViewerRegion*`.
However, the "Happy Path" instructions patch the header to use `const&` and provide a linker alias (`/ALTERNATENAME`) to make it link against the Snapshot's object files.

## The Mystery
Why does changing `T*` to `T* const&` prevent a crash, and why is the alias needed?
1.  **Symbol Mismatch**: The Snapshot was compiled with `T*`. The Local Mod is compiled with `T* const&`. This generates different mangled names in MSVC/Boost.
    *   Snapshot (Wanted): `...PEAV...` (Pointer)
    *   Local (Have): `...AEBQEAV...` (Reference to Pointer)
    *   *Correction*: In the investigation, the Snapshot expects `PEAV` (Pointer), but the Local Mod generates `AEBQEAV`. The link failure occurs because the Local Mod calls the signal with the `Ref` signature, but the Snapshot (which defines the signal's storage/slot mechanisms in some shared contexts?) might be expecting the `Ptr` signature.
    *   Actually, in the Repro:
        *   **App (Mod)** calls `trigger_signal`. It sees the signature `void(signal<void(int* const&)>&)`. It generates a symbol reference for this signature.
        *   **Lib (Snapshot)** defines `trigger_signal`. It sees the signature `void(signal<void(int*)>&)`. It exports a symbol for this signature.
        *   **Linker Error**: "Undefined symbol [App's Wanted Symbol]".

2.  **The Fix (Aliasing)**:
    By using `/ALTERNATENAME:Wanted=Have`, we tell the linker "When App asks for the Ref-signature function, give it the Ptr-signature function from Lib".

3.  **Runtime Behavior**:
    *   **ABI Compatibility**: `T*` is a pointer (value). `T* const&` is a reference to a pointer (address of a value).
    *   Normally, aliasing these would crash because of indirection levels (passing `&Ptr` instead of `Ptr`).
    *   **Hypothesis**: `boost::signals2` might be collapsing the reference or handling the invocation in a way that, for the generated code, the *calling convention* for the signal invocation ends up being compatible, OR the specific usage in `LLViewerRegion` (passing `this`) allows it to work.
    *   **Crash without `const&`**: If we *don't* use `const&` in the local header, we match the Snapshot's `T*`. But then why did the user report a crash?
        *   Maybe the Snapshot *actually* uses `const&` internally in some compilation units but not others?
        *   Or maybe the crash is unrelated to the signature itself but to how `boost` matches slots?

## Reproduction Strategy (Status: Ready to Resume)
We created a minimalist reproduction to isolate the behavior from the Viewer bloat.

### Files
*   `asdf/repro_app.cpp`: Hijacks `llappviewerwin32.cpp`. Simulates the Local Mod (Caller). Uses `int* const&`.
*   `asdf/repro_lib.cpp`: Hijacks `fsdata.cpp`. Simulates the Snapshot (Callee). Uses `int*`.
*   `asdf/polyfills/repro.link.rsp`: Contains the `/ALTERNATENAME` directive.

### Steps to Resume
1.  **Setup Environment**:
    ```bash
    cd asdf
    ../experiments/tpvm.sh init ../_snapshot/fs-7.2.2-avx2
    ../experiments/tpvm.sh mod llappviewerwin32.cpp
    ../experiments/tpvm.sh mod fsdata.cpp
    ```
2.  **Prepare Files**:
    *   Overwrite `asdf/llappviewerwin32.cpp` with content of `asdf/repro_app.cpp`.
    *   Overwrite `asdf/fsdata.cpp` with content of `asdf/repro_lib.cpp`.
3.  **Compile**:
    ```bash
    ../experiments/tpvm.sh compile llappviewerwin32.cpp fsdata.cpp
    ```
    *   *Verify*: Check `ls -l *.obj`. If `fsdata.cpp.obj` is large/old, the overwrite failed. It should be small.
4.  **Link**:
    ```bash
    ../llvm/bin/clang++ @env/link.rsp @polyfills/repro.link.rsp llappviewerwin32.cpp.obj fsdata.cpp.obj -o repro.exe
    ```
5.  **Run**:
    ```bash
    wine repro.exe
    ```

## Automation Goal
Create a script `experiments/auto_resolve_links.py` that:
1.  Runs the link command.
2.  Parses "Undefined symbol" errors.
3.  Scans provided `.obj` files using `llvm-nm`.
4.  Finds symbols that match except for `const&` variations (fuzzy match).
5.  Generates the `link.rsp` with `/ALTERNATENAME`.
6.  Retries linking.

## Mistakes & Lessons
1.  **Environment Persistence**: `tpvm.sh init` must be run if the session resets. The `env/` directory is ephemeral.
2.  **Pathing**: Always be explicit with paths. `cd asdf` then run commands relative to it.
3.  **File Overwrites**: `overwrite_file_with_block` might fail silently or interact poorly with `tpvm.sh mod` if done in the wrong order. Always verify file content/timestamp before compiling.
4.  **Tool Chaining**: Long bash chains can obscure failures. Run atomic commands when debugging.
