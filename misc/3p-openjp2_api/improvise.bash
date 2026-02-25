#!/bin/bash

openjp2_dir=$(dirname "${BASH_SOURCE}")

function verify_openjp2_from_packages_json() {
  local tarball=$1 json=${2:-$1.json}
  jq --arg tarball "$tarball" -r '.openjp2_api.hash + "\t" + $tarball' $json \
    | tr -d '\r' | tee /dev/stderr | md5sum --strict --check
}

provision_openjpeg_dummy() {(
  local cache_dir="$1"
  test -d "$cache_dir" || { echo "env cache_dir('$cache_dir') not found" >&2 ; return 15 ; }

  mkdir -p /tmp/openjpeg-dummy/LICENSES  /tmp/openjpeg-dummy/lib/release
  cat > /tmp/openjpeg-dummy/autobuild-package.xml <<'EOF'  
  <?xml version="1.0"?>  
<llsd>
<map>
    <key>build_id</key>
    <string>1</string>
    <key>configuration</key>
    <string>default</string>
    <key>manifest</key>
    <array>
      <string>LICENSES/openjpeg.txt</string>
      <string>lib/release/openjp2.lib</string>
      <string>lib/release/openjp2.dll</string>
    </array>
    <key>package_description</key>
    <map>
      <key>copyright</key>
      <string>no-op placeholder (see openjp2_api)</string>
      <key>description</key>
      <string>openjpeg-dummy (see openjp2_api which is used instead)</string>
      <key>license</key>
      <string>BSD</string>
      <key>license_file</key>
      <string>LICENSES/openjpeg.txt</string>
      <key>name</key>
      <string>openjpeg</string>
      <key>version</key>
      <string>1</string>
    </map>
    <key>platform</key>
    <string>windows64</string>
    <key>type</key>
    <string>metadata</string>
    <key>version</key>
    <string>1</string>
  </map>
</llsd>
EOF
  echo "dummy" > /tmp/openjpeg-dummy/LICENSES/openjpeg.txt  
  llvm-lib -llvmlibempty -out:/tmp/openjpeg-dummy/lib/release/openjp2.lib
  touch /tmp/openjpeg-dummy/lib/release/openjp2.dll
  ( cd /tmp/openjpeg-dummy/ && tar -cjf $cache_dir/openjpeg-v0.0.0-common-dummy.tar.bz2 * )
  tar tvf $cache_dir/openjpeg-v0.0.0-common-dummy.tar.bz2 >&2
  echo "$cache_dir/openjpeg-v0.0.0-common-dummy.tar.bz2 created if dummy/placeholder for original openjpeg autobuild is needed..." >&2
  return 0
)}

provision_openjp2() {(
  set -Euo pipefail
  local cache_dir="$1"
  test -d "$cache_dir" || { echo "env cache_dir('$cache_dir') not found" >&2 ; return 15 ; }

  # export tag=v2.5.1 commit=ae46a8d
  export tag=v2.5.3 commit=210a8a5

  tarball=$cache_dir/openjp2_api-$tag.$commit.tar.bz2

  cd "$openjp2_dir"
  mkdir -pv stage
  envsubst < autobuild-package.xml.in > stage/autobuild-package.xml
  touch --reference=autobuild-package.xml.in stage/autobuild-package.xml
  ls -l stage

  mkdir -pv stage/include/openjpeg stage/LICENSES

  # openvr repo is hundreds of megabytes... we just need LICENSE, openvr.h and src for static embedding

test -s openjpeg/src/lib/openjpeg.h || (
    export MSYS_NO_PATHCONV=1
    mkdir openjpeg
    cd openjpeg
    git init --initial-branch main
    git remote add origin https://github.com/uclouvain/openjpeg.git
    
    # Enable sparse-checkout and define exact paths (root file + specific folder)
    git sparse-checkout set --no-cone /LICENSE /src/lib/openjp2/

    # Fetch only the specific commit (metadata/tree only, no file contents yet)
    git fetch --filter=blob:none --depth 1 origin 210a8a5690d0da66f02d49420d7176a21ef409dc

    # Checkout hydrates ONLY the paths defined in sparse-checkout
    git -c advice.detachedHead=false checkout FETCH_HEAD
)

#  (
#     mkdir openjpeg
#     cd openjpeg
#     git init
#     git remote add origin https://github.com/uclouvain/openjpeg.git
#     git fetch --filter=blob:none --depth 1 origin 210a8a5
#     git checkout FETCH_HEAD
#  )

# git clone https://github.com/uclouvain/openjpeg.git --depth 1 --single-branch --no-progress
# git -C openjpeg checkout 210a8a5 || exit 33
  # git clone https://github.com/uclouvain/openjpeg --depth 1 --filter=blob:none --sparse || true
  # git -C openvr sparse-checkout set headers src || true
 ( cd openjpeg && cp --parents -av LICENSE src/lib/openjp2/{*.h,*.c} ../stage/include/openjpeg/ )
  
  #touch --reference=src/lib/openjp2/openjpeg.h stage/include/openjpeg/opj_config.h
  #touch stage/include/openjpeg/opj_config_private.h
  
  cp -av stage/include/openjpeg/LICENSE stage/LICENSES/openjp2.txt
  cp -av openjp2_api.h stage/include/openjpeg.h

  FILES=(
   autobuild-package.xml
   LICENSES/openjp2.txt
   include/openjpeg.h
  )

  for x in ${FILES[@]} ; do test -s stage/$x || { echo "'$x' invalid" >&2 ; exit 38 ; } ; done || return 61

  #set -x
  tar --force-local -C stage -cjvf $tarball ${FILES[@]} include/openjpeg || return 62

  hash=($(md5sum $tarball))
  url="file:///$tarball"
  qualified="$(jq '.openjp2_api.url = $url | .openjp2_api.hash = $hash | .openjp2_api.version = $version' --arg url "$url" --arg hash "$hash" --arg version "$tag.$commit" meta/packages-info.json)"

  test ! -s $tarball.json || echo "$qualified" | diff $tarball.json - || true

  echo "$qualified" | tee $tarball.json

  verify_openjp2_from_packages_json $tarball $tarball.json \
    || { echo "error verifying provisioned tarball $tarball / $tarball.json" >&2 ; return 68 ; }

  return 0

)}
