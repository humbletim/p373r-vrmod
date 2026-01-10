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
# Initialize the environment (generates shim, wchar.rsp, etc.)
asdf_tpvm init ../_snapshot/fs-7.2.2-avx2
```

## Modding prep:
```bash
asdf_tpvm mod llviewerdisplay.cpp
```

## Compile
```bash
# Compile files (requires community include path for llviewerVR.cpp)
CXXFLAGS=-I$PWD/community run_in_asdf compile llviewerdisplay.cpp
```

## Link
```bash
asdf_tpvm link
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
```

NOTE: `asdf/screenshot-region.jpg` should become captured -- show it to the user using `frontend_verification_complete`

### notify opensim-core to quit
```bash
cd hypergrid-naturalist-observatory && echo 'quit' | bash ./species/opensim-core/rest-console/execute_opensim_console_command.sh
```
