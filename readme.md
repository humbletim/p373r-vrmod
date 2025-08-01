# P373R's VR Mod for Firestorm Viewer

*This repository contains the isolated source code for P373R's VR mod, originally released in 2019. It is maintained here to support the "Firestorm VR Mod" community edition, which is built using GitHub Actions. This repository provides a focused environment for understanding and experimenting with the VR mod's codebase.* 

This project is not a typical open-source endeavor. It exists as a minimal, additive patch set to the official Firestorm viewer. This approach is intentional to minimize merge conflicts with the rapidly evolving upstream codebase and to avoid the significant overhead of maintaining a full-fledged fork. The goal is to provide a stable, community-supported VR experience without becoming a separate Third-Party Viewer (TPV).

## Theory of Operation

The VR mod integrates OpenVR (SteamVR) into the Firestorm viewer to provide a stereoscopic 3D view and head tracking. The approach is designed to be as non-invasive as possible.

### High-Level Overview

The mod intercepts the final rendered frame from Firestorm's rendering pipeline, just before it would be displayed on the 2D monitor. This single image is then processed and submitted to the OpenVR compositor, which handles the stereoscopic rendering and distortion correction required for VR headsets. Head tracking data from the headset is used to adjust the in-world camera position and orientation.

### Key Files and Their Roles

The core logic of the VR mod is contained in a small number of files:

- **`p373r/`**:
  - **[`llviewerVR.h`](./p373r/llviewerVR.h) and [`llviewerVR.cpp`](./p373r/llviewerVR.cpp)**: These files contain the `llviewerVR` class, which encapsulates all the VR-specific logic. This includes initializing and shutting down the OpenVR runtime, creating the framebuffers for each eye, handling controller input, processing head tracking data, and submitting the final frames to the compositor.
  - **[`0001-P373R-6.6.8-baseline-diff.patch`](./p373r/0001-P373R-6.6.8-baseline-diff.patch)**: This patch file contains the necessary modifications to the upstream Firestorm source code, primarily in [*llviewerdisplay.cpp*](https://github.com/FirestormViewer/phoenix-firestorm/blob/master/indra/newview/llviewerdisplay.cpp), to hook the VR mod into the viewer's main render loop.

Also in this repo:
- **`sgeo-minimal/`**: This directory contains a popular community-contributed variation of the mod by user @Sgeo. It refines the original approach by automatically configuring VR settings based on the headset's capabilities, removing the need for manual tweaking via the F5 menu.
- **`misc/3p-openvr/`**: This directory contains helper scripts for packaging the OpenVR SDK into a format compatible with Firestorm's `autobuild` system. This is primarily for automating the build process and is not essential for understanding the core VR mod logic.

### The Rendering Pipeline

1. **Standard Render:** The Firestorm viewer renders the 3D scene and the 2D UI into a single framebuffer, just as it would for a standard monitor.
2. **Interception:** The patch in `llviewerdisplay.cpp` adds a call to the `llviewerVR` class at the end of the render loop.
3. **Frame Submission:** The `vrDisplay()` method in `llviewerVR.cpp` takes the rendered frame and copies it to a texture. This texture is then submitted to the OpenVR compositor for both the left and right eyes. The compositor handles the necessary transformations to display the image correctly in the headset.
4. **Stereoscopic Effect:** The stereoscopic 3D effect is achieved by slightly shifting the camera position for each eye's view. This is handled in the `ProcessVRCamera()` method, which adjusts the camera's position based on the user's IPD (interpupillary distance).

### The UI "Trick"

The mod does not implement a true 3D user interface. Instead, it uses a clever "pan and zoom" technique to make the existing 2D UI usable in VR. The user's mouse movements on the 2D desktop window are used to pan the view of the UI within the headset. This allows the user to access all of the standard Firestorm menus and windows, albeit in a way that requires a hybrid use of the headset and a traditional mouse and keyboard.

## Instructions for Integrating into a Viewer Source Tree

Please the [`p373r/BUILDING.md`](./p373r/BUILDING.md) file. The would be process is similar for integrating `sgeo-minimal/` instead.

For more detailed instructions on contributing, please see the [`CONTRIBUTING.md`](./CONTRIBUTING.md) file.

