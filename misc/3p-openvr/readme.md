### 3p-openvr helper stub

improvise.bash emerges an LL autobuild compatible openvr-vX.Y.Z.sha.tar.bz2 containing:
  - autobuild-package.xml
  - LICENSES/openvr.txt
  - include/openvr.h
  - lib/release/openvr_api.{dll,lib,viewer_manifest.patch}

NOTE: this 3p stuff is only needed if wanting to integrate as part of autobuild.xml configuration

to execute from a git+windows bash prompt for local development:
```sh
bash -c '. improvise.bash ; provision_openvr [output_dir]'
```

generated package could then be added as a custom SLOS/TPV 3p dependency using LL autobuild tool:
```sh
autobuild installables edit --archive [output_dir]/openvr-vX.Y.Z.sha.tar.bz2
```

see also:
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr)
  - currently [v1.6.10](https://github.com/ValveSoftware/openvr/tree/v1.6.10) commit point is used
- [humbletim/firestorm-gha](https://github.com/humbletim/firestorm-gha)
- [humbletim/p373r-vrmod](https://github.com/humbletim/p373r-vrmod)
- [SL/wiki/Autobuild_installables](https://wiki.secondlife.com/wiki/Autobuild_installables)
