# edge-compilation + opensim field test + screenshot verification

execute each of the following blocks as *separate bash invocations*.

## Set up Observatory-OpenSim
```bash
git clone https://github.com/metaverse-crossroads/hypergrid-naturalist-observatory
cd hypergrid-naturalist-observatory && make opensim-core
```

## Environment prep:
```bash
ln -sTf /usr/lib/llvm-19 /opt/humbletim/llvm
ln -sTf /opt/humbletim/llvm /app/llvm
# Initialize the environment (generates shim, wchar.rsp, etc.)
cd asdf && ../experiments/tpvm.sh init ../_snapshot/fs-7.2.2-avx2
```

## Modding prep:
```bash
cd asdf && ../experiments/tpvm.sh mod llviewerdisplay.cpp
```

## Compile
```bash
# Compile files
cd asdf && ../experiments/tpvm.sh compile llviewerdisplay.cpp
```

## Link
```bash
cd asdf && ../experiments/tpvm.sh link
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
cd asdf && TIMEOUT=90s ./manual_screenshot.sh --grid http://localhost:9000 --login Test User password
```

NOTE: `asdf/screenshot-region.jpg` should become captured -- show it to the user using `frontend_verification_complete`

### notify opensim-core to quit
```bash
cd hypergrid-naturalist-observatory && echo 'die' | bash ./species/opensim-core/rest-console/execute_opensim_console_command.sh
```
