file build/placeholder-icon.ico
set -a
. ${snapshot_dir}/metadata/env.d/build_vars.env
CompanyName=${GITHUB_REPOSITORY}
FileDescription=${GITHUB_WORKFLOW}
ProductName=$(echo "${GITHUB_WORKFLOW_REF}" | tr '@' '#')
ProductVersion=${version_full}
FileVersion=${viewer_version}
InternalName=${viewer_channel}
LegalCopyright=$(date +"%Y.%m.%d.%H:%M:%S" )
icon=build/placeholder-icon.ico
sed -E "$(echo '
        s@\b((FILE|PRODUCT)VERSION\s+(([0-9]+.){3}))[0-9]{5}@\10@g;
        s@(VALUE (.)CompanyName.), .*@    \1, \2${CompanyName}\2    @;
        s@(VALUE (.)ProductName.), .*@    \1, \2${ProductName}\2    @;
        s@(VALUE (.)ProductVersion.), .*@ \1, \2${ProductVersion}\2 @;
        s@(VALUE (.)InternalName.), .*@   \1, \2${InternalName}\2   @;
        s@(VALUE (.)FileDescription.), .*@\1, \2${FileDescription}\2@;
        s@(VALUE (.)FileVersion.), .*@    \1, \2${FileVersion}\2    @;
        s@(VALUE (.)LegalCopyright.), .*@ \1, \2${LegalCopyright}\2 @;
        s@firestorm_icon.ico@${icon}@;
' | envsubst)" < ${snapshot_dir}/source/viewerRes.rc | tee build/viewerRes.rc | \
  llvm/bin/clang @fs-devtime/compile.rsp -vfsoverlay build/vfsoverlay.yml -x c++ -E - -I$snapshot_dir/source/newview/res | \
  llvm/bin/llvm-rc -c1252 -I$snapshot_dir/source/newview/res -Fobuild/viewerRes.rc.res -

ls -l build/icon.ico build/viewerRes* 