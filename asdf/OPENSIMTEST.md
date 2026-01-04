# edge-compilation + opensim field test + screenshot verification

Jules: execute each of the following blocks as *separate bash invocations*.

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

## Screenshot -- STATE_LOGIN_WAIT
```bash
cd asdf && ./screenshot.sh # this should emerge screenshot.jpg
cd asdf && mv -v screenshot.jpg screenshot-init.jpg
# show asdf/screenshot-init.jpg to the user using "frontend_verification_complete"
```

## Screenshot -- STATE_STARTED
### make sure opensim-core is running to connect to...
```bash
cd hypergrid-naturalist-observatory && make run-opensim-core &
```

### capture screenshot of successful logging in and entering region...
```bash
cd asdf && ./screenshot.sh --grid 127.0.0.1:9000 --login Test User password
cd asdf && mv -v screenshot.jpg screenshot-region.jpg
# show asdf/screenshot-region.jpg to the user using "frontend_verification_complete"
```

### notify opensim-core to quit
```bash
cd hypergrid-naturalist-observatory && echo 'quit' | ./species/opensim-core/rest-console/connect_opensim_console_session.sh
sleep 10
ps faux | grep -E '[d]otnet' # just to see if it exited
```
