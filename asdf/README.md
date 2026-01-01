# these might already exist but to make sure run:
ln -sTf /usr/lib/llvm-19 /opt/humbletim/llvm
ln -sTf /opt/humbletim/llvm /app/llvm

# all these commands are meant to run from this asdf folder
# SO from the repo root you would invoke as `cd asdf && ../experiments/tpvm.sh ...`

# Initialize the environment (generates shim, wchar.rsp, etc.)
../experiments/tpvm.sh init ../_snapshot/fs-7.2.2-avx2

# Mod files (copies from snapshot)
../experiments/tpvm.sh mod llstartup.cpp
../experiments/tpvm.sh mod fsdata.cpp
../experiments/tpvm.sh mod llviewerregion.cpp
../experiments/tpvm.sh mod llviewerregion.h

# Patch files
patch -p3 < fsdata.cpp.patch
patch -p3 < llstartup.cpp.patch
patch -p3 < llviewerregion.cpp.patch
patch -p3 < llviewerregion.h.patch

# Compile files
../experiments/tpvm.sh compile llstartup.cpp
../experiments/tpvm.sh compile fsdata.cpp
LDFLAGS=@polyfills/llviewerregion.link.rsp ../experiments/tpvm.sh compile llviewerregion.cpp

# Link
../experiments/tpvm.sh link

# diff -u snapshot/source/newview/ llstartup.cpp > llstartup.cpp.patch
# diff -u snapshot/source/newview/ fsdata.cpp > fsdata.cpp.patch
