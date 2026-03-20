# edge-compilation + opensim field test + screenshot verification

execute each of the following blocks as *separate bash invocations*.

## 0. Pre-flight Checks & Setup

### System Integrity Check
**CRITICAL:** Verify that `/opt/humbletim` exists.
If `/opt/humbletim` does **NOT** exist, this is a **FATAL SYSTEM ERROR**.
**STOP IMMEDIATELY**. Do not attempt to create the directory or install dependencies manually. Report the environment failure.

### One-time Symlink Setup
Ensure the following LLVM toolchain symlinks are established:
```bash
ln -sTf /usr/lib/llvm-19 /opt/humbletim/llvm
ln -sTf /opt/humbletim/llvm /app/llvm
```

### Execution Helper
Define this helper function to simplify commands run within the `asdf` context:
```bash
asdf_tpvm() {
  (cd asdf && ../experiments/tpvm.sh "$@")
}
```

## Set up Observatory-OpenSim
```bash
git clone https://github.com/metaverse-crossroads/hypergrid-naturalist-observatory
cd hypergrid-naturalist-observatory && make opensim-core
```

## Environment prep:
```bash
asdf_tpvm init ../_snapshot/fs-7.2.2-avx2
```

<!-- ALTERNATIVE for original P373R vrmod:
asdf_tpvm mod llviewerdisplay.cpp
patch --directory=. --merge <  ../sgeo-minimal/0001-sgeo_min_vr_7.1.9-baseline-diff.patch
asdf_tpvm compile llviewerdisplay.cpp
asdf_tmp link
# produces fs-open.exe w/community vrmod edition changes
-->

## Modding prep:
```bash
# NOTE: iff snapshot llviewerdisplay.cpp *already* contains legacy P373R vrmods
# then you may need to first back those changes out before attempting to apply
# the new ultra-minimalist "three lines total" vrmodding integration pattern
# see: patches/llviewerdisplay.cpp.undo-p373r.patch
# verify: there areno P373R tags in llviewerdisplay.cpp
asdf_tpvm patch llviewerdisplay.cpp

# these should all "just work" if using a compatible snapshot
asdf_tpvm patch llviewercamera.cpp
asdf_tpvm patch llagentcamera.cpp
asdf_tpvm patch llvoavatar.cpp
# asdf_tpvm patch fsdata.cpp
asdf_tpvm patch llstartup.cpp
asdf_tpvm patch pipeline.cpp
```

## Compile
```bash
make -j fs-7.2.2-preview-avx2.exe
```

## Screenshot

### make sure opensim-core is running to connect to...
```bash
cd hypergrid-naturalist-observatory && make run-opensim-core -- -console rest 2>&1 > /tmp/opensim.log &
```

### you can verify opensim ready and available by issuing
```bash
cd hypergrid-naturalist-observatory && echo 'show uptime' | bash ./species/opensim-core/rest-console/execute_opensim_console_command.sh
```

### capture screenshot of successful logging in and entering region...
```bash
# Note: Increased timeout to accommodate login latency
cd asdf && TIMEOUT=180s ./manual_screenshot.sh --grid http://localhost:9000 --login "Test User" password
# (NOTES FOR FUTURE TESTING: DUMMYHMD=1448x1808 EXEROOT=snapshot/runtime WINEPATH=snapshot/runtime wine ./fs-7.2.2-preview-avx2.exe)

```

NOTE: `asdf/screenshot-region.jpg` should become captured -- show it to the user using `frontend_verification_complete`

### notify opensim-core to quit
```bash
cd hypergrid-naturalist-observatory && echo 'quit' | bash ./species/opensim-core/rest-console/execute_opensim_console_command.sh
```
