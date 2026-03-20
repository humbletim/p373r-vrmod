# Polyfills & Hackarounds

This directory contains modular "polyfills" and workarounds used to bridge the gap between the MSVC-targeted Second Life viewer codebase and the LLVM/Clang toolchain on Linux (targeting Windows via Wine/cross-compilation).

## Components

### `boost_facade`
*   **Files:** `boost_facade.hpp`, `boost_facade.rsp`, `boost_facade.link.rsp`
*   **Purpose:** The viewer code uses specific Boost patterns (and potentially old Boost versions) that conflict or require adjustment when compiling with modern Clang/LLVM against the Windows SDK. This facade re-namespaces `boost` to `actualboost` and then reconstructs a `boost` namespace (`tsoob`) with necessary shims, particularly for `signals2`.
*   **Linker Shim:** `boost_facade.link.rsp` provides symbol aliasing (`/ALTERNATENAME`) to map the mangled names expected by the viewer code (using the facade) to the actual symbols present in the precompiled libraries.

### `wchar_fixes` (Experimental/Unused)
*   **Files:** `wchar_fixes.hpp`, `wchar_fixes.rsp`
*   **Purpose:** Address potential conflicts between native `wchar_t` and `unsigned short` when dealing with Windows headers and UTF-16 strings. Not currently required for `fsdata.cpp`.

### `forced_headers` (Experimental/Unused)
*   **Files:** `forced_headers.hpp`, `forced_headers.rsp`
*   **Purpose:** A collection of Viewer-specific headers that were previously force-included to resolve dependency issues. Modularized here to allow granular inclusion if needed.
