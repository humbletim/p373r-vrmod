#!/bin/bash

community_vrmod_dir=$(dirname "${BASH_SOURCE}")

function verify_community_vrmod_from_packages_json() {
  local tarball=$1 json=${2:-$1.json}
  jq --arg tarball "$tarball" -r '.["community-vrmod"].hash + "\t" + $tarball' $json \
    | tr -d '\r' | md5sum --strict --check
}

function provision_community_vrmod() {(
  set -Euo pipefail
  local cache_dir="$1"
  test -d "$cache_dir" || { echo "env cache_dir('$cache_dir') not found" >&2 ; return 15 ; }

  export tag=v7.2.3 commit=`git describe --always`

  tarball=$cache_dir/community-vrmod-$tag.$commit.tar.bz2

  cd "$community_vrmod_dir"
  mkdir -pv stage
  envsubst < autobuild-package.xml.in > stage/autobuild-package.xml
  touch --reference=autobuild-package.xml.in stage/autobuild-package.xml
  ls -l stage

  (
    set -Eou pipefail
    STAGE=$PWD/stage
    cd stage
    mkdir -pv LICENSES include/community-vrmod # lib/release include 
    cp -av ../community-vrmod.txt LICENSES/community-vrmod.txt
    ( cd ../../../community/ && cp --parents -av llviewerVR*.* vrmod.llviewerdisplay.hpp patches/ $STAGE/include/community-vrmod/ )
    ( cd ../../../community/../asdf/ && cp --parents -av humbletim/{xopenvr.hpp,opengl.hpp} $STAGE/include/community-vrmod/ )
  )

  FILES=(
   autobuild-package.xml
   LICENSES/community-vrmod.txt
  #  include/community-vrmod/llviewerVR.h
  #  include/community-vrmod/llviewerVR.cpp
  #  include/community-vrmod/llviewerVR.vrmod_settings.c++
  )

  for x in ${FILES[@]} ; do test -s stage/$x || { echo "'$x' invalid" >&2 ; exit 38 ; } ; done || return 61

  #set -x
  tar --force-local -C stage -cjvf $tarball ${FILES[@]} include/community-vrmod || return 62

  hash=($(md5sum $tarball))
  url="file:///$tarball"
  qualified="$(jq '.["community-vrmod"].url = $url | .["community-vrmod"].hash = $hash | .["community-vrmod"].version = $version' --arg url "$url" --arg hash "$hash" --arg version "$tag.$commit" packages-info.json)"

  test ! -s $tarball.json || echo "$qualified" | diff $tarball.json - || true

  echo "$qualified" | tee $tarball.json

  verify_community_vrmod_from_packages_json $tarball $tarball.json \
    || { echo "error verifying provisioned tarball $tarball / $tarball.json" >&2 ; return 68 ; }

  return 0
)}
