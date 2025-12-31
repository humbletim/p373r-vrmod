
../experiments/tpvm.sh init ../_snapshot/fs-7.2.2-avx2
../experiments/tpvm.sh mod llstartup.cpp
../experiments/tpvm.sh mod fsdata.cpp

patch -p3 < fsdata.cpp.patch
patch -p3 < llstartup.cpp.patch

CXXFLAGS=@llstartup.compile.rsp ../experiments/tpvm.sh compile llstartup.cpp
CXXFLAGS=@fsdata.compile.rsp ../experiments/tpvm.sh compile fsdata.cpp
LDFLAGS=@fsdata.link.rsp ../experiments/tpvm.sh link

# diff -u snapshot/source/newview/ llstartup.cpp > llstartup.cpp.patch
# diff -u snapshot/source/newview/ fsdata.cpp > fsdata.cpp.patch
