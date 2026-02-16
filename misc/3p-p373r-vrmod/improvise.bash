#!/bin/bash

p373r_vrmod_dir=$(dirname "${BASH_SOURCE}")

function verify_p373r_vrmod_from_packages_json() {
  local tarball=$1 json=${2:-$1.json}
  jq --arg tarball "$tarball" -r '.["p373r-vrmod"].hash + "\t" + $tarball' $json \
    | tr -d '\r' | md5sum --strict --check
}

function provision_p373r_vrmod() {(
  set -Euo pipefail
  local cache_dir="$1"
  test -d "$cache_dir" || { echo "env cache_dir('$cache_dir') not found" >&2 ; return 15 ; }

  export tag=vX.Y.Z commit=0000000

  tarball=$cache_dir/p373r-vrmod-$tag.$commit.tar.bz2

  cd "$p373r_vrmod_dir"
  mkdir -pv stage
  envsubst < autobuild-package.xml.in > stage/autobuild-package.xml
  touch --reference=autobuild-package.xml.in stage/autobuild-package.xml
  ls -l stage

  (
    set -Eou pipefail
    cd stage
    mkdir -pv LICENSES # lib/release include 
    cp -av ../p373r-vrmod.txt LICENSES/p373r-vrmod.txt
  )

  FILES=(
   autobuild-package.xml
   LICENSES/p373r-vrmod.txt
   # include/openvr.h
   # lib/release/vrmod_api.{dll,lib}
  )

  for x in ${FILES[@]} ; do test -s stage/$x || { echo "'$x' invalid" >&2 ; exit 38 ; } ; done || return 61

  #set -x
  tar --force-local -C stage -cjvf $tarball ${FILES[@]} || return 62

  hash=($(md5sum $tarball))
  url="file:///$tarball"
  qualified="$(jq '.["p373r-vrmod"].url = $url | .["p373r-vrmod"].hash = $hash | .["p373r-vrmod"].version = $version' --arg url "$url" --arg hash "$hash" --arg version "$tag.$commit" meta/packages-info.json)"

  test ! -s $tarball.json || echo "$qualified" | diff $tarball.json - || true

  echo "$qualified" | tee $tarball.json

  verify_p373r_vrmod_from_packages_json $tarball $tarball.json \
    || { echo "error verifying provisioned tarball $tarball / $tarball.json" >&2 ; return 68 ; }

  return 0
)}
