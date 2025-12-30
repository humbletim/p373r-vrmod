# Jules Build System

This directory contains simplified scripts for iterating on Firestorm Viewer code units ("edge compilation").

## Structure

- `temp/`: Working directory for modified source files.
- `build/`: Output directory for object files and the final executable.
- `patches/`: Directory for patch files to be applied during initialization.
- `init_override.sh`: Initializes a source file for modification.
- `compile.sh`: Compiles a modified source file.
- `link.sh`: Links the executable with the modified object files.
- `screenshot.sh`: Runs the viewer and captures a screenshot.

## Prerequisites

The scripts assume the standard build environment (`_snapshot`, `fs-devtime`, `llvm`, etc.) is already set up at the repository root.

## Workflow Example

1. **Initialize an override:**
   This copies the source file from the snapshot to `jules/temp/`, flattening the path.
   ```bash
   ./jules/init_override.sh indra/newview/llstartup.cpp
   ```
   *Output:* `jules/temp/indra_newview_llstartup.cpp`

2. **Modify the file:**
   Edit `jules/temp/indra_newview_llstartup.cpp` with your changes.

3. **Compile:**
   This compiles the modified source into an object file in `jules/build/`.
   ```bash
   ./jules/compile.sh jules/temp/indra_newview_llstartup.cpp
   ```

4. **Link:**
   This generates a custom response file (`llobjs.custom.rsp`) replacing the original object file with your local version, then links the executable.
   ```bash
   ./jules/link.sh
   ```
   *Output:* `jules/build/winxtropia-custom.exe`

5. **Test:**
   This runs the custom executable in the Wine/Xvfb environment and captures a screenshot once the login screen is reached.
   ```bash
   export PATH=$PATH:/usr/bin
   ./jules/screenshot.sh
   ```
   *Output:* `jules_screenshot.jpg` and logs in `jules/build/steamuser/...`

## Notes

- **RSP Modification:** The `link.sh` script works by copying the original `llobjs.rsp` and performing text substitution to point to the new object files. It matches based on the filename suffix (e.g., matching `newview/llstartup.cpp.obj`).
- **Includes:** `compile.sh` includes the snapshot source directories. If you encounter missing headers, you may need to add specific subdirectories to the `-I` flags in `compile.sh`.
