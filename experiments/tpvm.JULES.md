# Session Post-Mortem & Restoration Guide: `tpvm.sh` Development

## 1. Baseline Restoration & Verification
To restore the session to a known working state and verify the baseline environment, run the following commands exactly as originally prescribed.

```bash
# 1. Build Verification
export PATH=$PATH:/usr/bin snapshot_dir=_snapshot/fs-7.2.2-avx2 base=fs-7.2.2-avx2  ./experiments/fs/winxtropia-custom/ubuntu.test.build.bash

# 2. Runtime Verification (Screencapture)
export PATH=$PATH:/usr/bin snapshot_dir=_snapshot/fs-7.2.2-avx2 base=fs-7.2.2-avx2  ./experiments/fs/winxtropia-custom/ubuntu.test.screencapture.bash
```

## 2. Implementation Status (`tpvm.sh`)

**Current State: PARTIAL / UNVERIFIED LINKING**

*   `init`: **Verified**. Correctly sets up `env/`, `.rsp` files, and symlinks (`snapshot`, `winsdk`).
*   `mod`: **Verified**. Correctly finds and copies files.
*   `compile`: **Verified**. Correctly compiles local C++ files using snapshot flags.
*   `link`: **UNVERIFIED**.
    *   The script currently attempts to use `clang++` as a driver for the link step (mimicking `build.bash`).
    *   **This specific implementation has NOT been run or verified.** The session was halted before verification could occur.

## 3. Technical Issues & Misconceptions

### The `lld-link` vs `clang++` Driver Issue
*   **Attempt**: I initially tried to invoke `lld-link` directly.
*   **Failure**: The command failed with errors regarding unknown flags (e.g., `-ivfsoverlay` which are Clang driver flags, not Linker flags) and issues processing the VFS overlay.
*   **Correction/Pivot**: I switched the script to use `clang++` to drive the linker, hoping it would handle the `.rsp` flags (which mix compiler and linker options) correctly.
*   **Correction on Tool Capabilities**: I incorrectly stated `lld-link` does not support `/vfsoverlay`. It does. My failure was likely due to:
    1.  Passing Clang *driver* flags (from `winsdk.rsp`) to the *linker* executable.
    2.  Incorrect syntax or path resolution for the VFS overlay in my specific `lld-link` invocation.

### Winsdk Symlink
*   The `winsdk.rsp` file contains relative paths (`winsdk/crt/...`).
*   To allow these to resolve from CWD, `tpvm.sh init` creates a symlink: `./winsdk -> $REPO_ROOT/winsdk`.

## 4. `tpvm.sh` Usage (Intended)
**Warning: `link` step is unproven.**

1.  `./tpvm.sh init _snapshot/fs-7.2.2-avx2`
2.  `./tpvm.sh mod llviewerdisplay.cpp`
3.  `./tpvm.sh compile`
4.  `./tpvm.sh link` (Expect issues; duplicate symbol filtering is implemented but untested; VFS handling via clang++ driver is untested).
