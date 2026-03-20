### 3p-openjp2_api helper stub

improvise.bash emerges an LL autobuild compatible static openjpeg openjp2_api-vX.Y.Z.sha.tar.bz2 containing:
  - autobuild-package.xml
  - LICENSES/openjp2.txt
  - include/openjp2_api/src/lib/openjp2/{*.h,*.c}
  - include/openjpeg.h

include/openjpeg.h is a special single-file header amalgamating wrapper:
  - include like openjpeg.h when wanting OpenJPEG declarations 
  - (in one code unit) define `OPENJP2_API_IMPLEMENTATION` first before including
    - NOTE: this avoids depending on or shipping a separate openjp2.dll!

to execute from a git+windows bash prompt for local development:
```sh
bash -c '. improvise.bash ; provision_openjp2_api [output_dir]'
```

generated package could then be added as a custom SLOS/TPV 3p dependency using LL autobuild tool:
```sh
autobuild installables add --archive [output_dir]/openjp2_api-vX.Y.Z.sha.tar.bz2
# autobuild installables edit --archive [output_dir]/openjp2_api-vX.Y.Z.sha.tar.bz2
```

see also:
- [uclouvain/openjpeg](https://github.com/uclouvain/openjpeg)
  - currently [v2.5.3](https://github.com/uclouvain/openjpeg/tree/v2.5.3) commit point is used
- [humbletim/firestorm-gha](https://github.com/humbletim/firestorm-gha)
- [humbletim/p373r-vrmod](https://github.com/humbletim/p373r-vrmod)
- [SL/wiki/Autobuild_installables](https://wiki.secondlife.com/wiki/Autobuild_installables)
