## Instructions for Integrating into a Viewer Source Tree

1. **Apply the Patch:** Apply the `0001-P373R-6.6.8-baseline-diff.patch` to `indra/newview/llviewerdisplay.cpp`.
2. **Copy Core Files:** Copy `llviewerVR.cpp` and `llviewerVR.h` into the `indra/newview` directory.
3. **Update CMake:** Modify `indra/newview/CMakeLists.txt` to include the path to `openvr.h` and to link against `openvr_api.lib`.
4. **Build:** Configure and build the viewer as usual.
5. **Deploy DLL:** Copy `openvr_api.dll` into the same directory as the compiled viewer executable.

For more detailed instructions on contributing, please see the [`CONTRIBUTING.md`](../CONTRIBUTING.md) file.
