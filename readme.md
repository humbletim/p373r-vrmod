*This is P373R's VR mod source from original .rar release as a github repo.*
*It's used by [humbletim/firestorm-gha](https://github.com/humbletim/firestorm-gha) as part
of building the unofficial "Firetorm VR Mod" community edition.*

## Instructions for integrating P373R mods into an existing SLOS/TPV viewer source tree:
1. **Code changes** -- within `indra/newview`:
   - apply `0001-P373R-6.6.8-baseline-diff.patch` to `indra/newview/llviewerdisplay.cpp`
   - copy `llviewerVR.cpp` and `llviewerVR.h` into `indra/newview`
2. **CMake changes** -- `indra/neview/CMakeLists.txt`:
   - add path for `openvr.h` as viewer target include directory
   - add `openvr_api.lib` as a viewer target link library dependency
3. Configure and build the viewer like usual
4. Copy `openvr_api.dll` alongside the compiled viewer .exe

For (1) the `patch` utility (available as part of standard git+windows bash prompt) could be used:
```sh
patch --directory=indra/newview --ignore-whitespace --verbose --merge -p1 < p373r-vrmod/0001-P373R-6.6.8-baseline-diff.patch
```

Notes re: OpenVR dependency:
- Later OpenVR SDK versions *might* work, [v1.6.10](https://github.com/ValveSoftware/openvr/tree/v1.6.10) is known to work
- See [./3p-openvr](./3p-openvr) folder for experimental 3p autobuild packaging
- To avoid copying `openvr_api.dll` around manually, see viewer source:
  - `indra/newview/viewer_manifest.py`
  - `cmake/Copy3rdPartyLibs.cmake`
- To include `openvr_api.dll` as part of generated installers, see viewer source:
  - `indra/newview/installers/windows/installer_template.nsi`

---

*Original notes from the .rar file release:*

### Firestorm_6.3.3_VR_Source/Readme.txt @ Nov 28  2019

> The Source is written in a way that it requires only some editing in the llviewerdisplay.cpp.
> All changed are marked with ###################P373R#####################  comments.
> I also included the openvr header and lib files you will need in the rar.
> For informations about the rest of the files you will need, read how to compile Firestorm.
> https://wiki.firestormviewer.org/fs_compiling_firestorm
> Have fun :D

- see https://gsgrid.de/firestorm-vr-mod/
- imported from http://gsgrid.net/Firestorm_6.3.3_VR_Source.rar
