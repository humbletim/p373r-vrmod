### 3p-p373r-vrmod helper stub

improvise.bash emerges an LL autobuild compatible p373r-vrmod-vX.Y.Z.sha.tar.bz2 containing:
  - autobuild-package.xml
  - LICENSES/p373r-vrmod.txt

NOTE: this 3p stuff is only needed if wanting to integrate as part of autobuild.xml configuration

to execute from a git+windows bash prompt for local development:
```sh
bash -c '. improvise.bash ; provision_p373r_vrmod [output_dir]'
```

generated package could then be added as a custom SLOS/TPV 3p dependency using LL autobuild tool:
```sh
autobuild installables edit --archive [output_dir]/p373r-vrmod-vX.Y.Z.sha.tar.bz2
```

see also:
- [humbletim/firestorm-gha](https://github.com/humbletim/firestorm-gha)
- [humbletim/p373r-vrmod](https://github.com/humbletim/p373r-vrmod)
