if not exist p373r-vrmod ( git clone https://github.com/humbletim/p373r-vrmod.git --single-branch --branch devtime )
call p373r-vrmod\experiments\setup-portables.bat
pg\bin\bash.exe -c ". p373r-vrmod/experiments/gha.winsdk.bash ; PATH=bin:llvm/bin:$PATH gha-winsdk-llvm-capture ; gha-winsdk-devtime-capture fs-7.1.13-devtime-avx2"
call p373r-vrmod\experiments\build-sgeo-minimal.bat
