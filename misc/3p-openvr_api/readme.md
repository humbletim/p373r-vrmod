### 3p-openvr_api helper stub

improvise.bash emerges an LL autobuild compatible static openvr_api-vX.Y.Z.sha.tar.bz2 containing:
  - autobuild-package.xml
  - LICENSES/openvr_api.txt
  - include/openvr_api/{headers,src}/*
  - include/openvr.h

include/openvr.h is a special single-file header amalgamating wrapper:
  - include like openvr.h when wanting OpenVR declarations 
  - (in one code unit) define `OPENVR_API_IMPLEMENTATION` first before including
    - NOTE: this avoids depending on / shipping a separate openvr_api.dll!

to execute from a git+windows bash prompt for local development:
```sh
bash -c '. improvise.bash ; provision_openvr_api [output_dir]'
```

generated package could then be added as a custom SLOS/TPV 3p dependency using LL autobuild tool:
```sh
autobuild installables edit --archive [output_dir]/openvr_api-vX.Y.Z.sha.tar.bz2
```

see also:
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr)
  - currently [v1.6.10](https://github.com/ValveSoftware/openvr/tree/v1.6.10) commit point is used
- [humbletim/firestorm-gha](https://github.com/humbletim/firestorm-gha)
- [humbletim/p373r-vrmod](https://github.com/humbletim/p373r-vrmod)
- [SL/wiki/Autobuild_installables](https://wiki.secondlife.com/wiki/Autobuild_installables)
