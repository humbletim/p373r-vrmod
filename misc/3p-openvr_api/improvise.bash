#!/bin/bash

openvr_api_dir=$(dirname "${BASH_SOURCE}")

function verify_openvr_api_from_packages_json() {
  local tarball=$1 json=${2:-$1.json}
  jq --arg tarball "$tarball" -r '.openvr_api.hash + "\t" + $tarball' $json \
    | tr -d '\r' | md5sum --strict --check
}

provision_openvr_api() {(
  set -Euo pipefail
  local cache_dir="$1"
  test -d "$cache_dir" || { echo "env cache_dir('$cache_dir') not found" >&2 ; return 15 ; }

  # export tag=v2.5.1 commit=ae46a8d
  export tag=v1.6.10 commit=8eaf723

  tarball=$cache_dir/openvr_api-$tag.$commit.tar.bz2

  cd "$openvr_api_dir"
  mkdir -pv stage
  envsubst < autobuild-package.xml.in > stage/autobuild-package.xml
  touch --reference=autobuild-package.xml.in stage/autobuild-package.xml
  ls -l stage

  mkdir -pv stage/include/openvr_api stage/LICENSES

  # openvr repo is hundreds of megabytes... we just need LICENSE, openvr.h and src for static embedding
  git clone https://github.com/ValveSoftware/openvr --branch v1.6.10 --single-branch --depth 1 --filter=blob:none --sparse
  git -C openvr sparse-checkout set headers src
  ( cd openvr && cp --parents -av {LICENSE,headers/openvr.h} src/{,**/}{*.h,*.cpp} ../stage/include/openvr_api/ )
  
  cp -av stage/include/openvr_api/LICENSE stage/LICENSES/openvr_api.txt
  cp -av openvr_api.hpp stage/include/openvr.h

  FILES=(
   autobuild-package.xml
   LICENSES/openvr_api.txt
   include/openvr.h
   include/openvr_api/
  )

  for x in ${FILES[@]} ; do test -s stage/$x || { echo "'$x' invalid" >&2 ; exit 38 ; } ; done || return 61

  #set -x
  tar --force-local -C stage -cjvf $tarball ${FILES[@]} || return 62

  hash=($(md5sum $tarball))
  url="file:///$tarball"
  qualified="$(jq '.openvr_api.url = $url | .openvr_api.hash = $hash | .openvr_api.version = $version' --arg url "$url" --arg hash "$hash" --arg version "$tag.$commit" meta/packages-info.json)"

  test ! -s $tarball.json || echo "$qualified" | diff $tarball.json - || true

  echo "$qualified" | tee $tarball.json

  verify_openvr_api_from_packages_json $tarball $tarball.json \
    || { echo "error verifying provisioned tarball $tarball / $tarball.json" >&2 ; return 68 ; }

  return 0

)}
