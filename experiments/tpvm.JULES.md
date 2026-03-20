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

**Current State: VERIFIED**

*   `init`: **Verified**. Correctly sets up `env/`, `.rsp` files, and symlinks (`snapshot`, `winsdk`).
*   `mod`: **Verified**. Correctly finds and copies files.
*   `compile`: **Verified**. Correctly compiles local C++ files using snapshot flags.
    *   Includes `mm.rsp` injection for SSE support.
    *   Includes `sgeo-minimal` includes for VR support.
*   `link`: **Verified**.
    *   Uses `clang++` driver to wrap `lld-link` execution, handling Clang-style flags in `.rsp` files.
    *   Successfully links `fs-7.2.2-avx2.exe` with local overrides.
    *   Filters `llobjs.rsp` to avoid duplicate symbols using a Python helper.

## 3. Technical Decisions

### The `lld-link` vs `clang++` Driver Issue
*   We use `clang++` to drive the linker because existing `.rsp` templates (e.g. `winsdk.rsp`, `link.common.rsp`) contain Clang driver flags (like `-Wl`, `-target`, `-isystem`) which are incompatible with `lld-link` direct invocation.
*   `clang++` correctly passes arguments to `lld-link` (via `-fuse-ld=lld`) on the Windows target.

### Winsdk Symlink
*   The `winsdk.rsp` file contains relative paths (`winsdk/crt/...`).
*   To allow these to resolve from CWD, `tpvm.sh init` creates a symlink: `./winsdk -> $REPO_ROOT/winsdk`.

## 4. `tpvm.sh` Usage (Verified)

1.  `./tpvm.sh init _snapshot/fs-7.2.2-avx2`
2.  `./tpvm.sh mod llviewerdisplay.cpp`
3.  `./tpvm.sh compile`
4.  `./tpvm.sh link`
    *   Result: `fs-*.exe` is created and matches functional expectations.
