#!/bin/bash
# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Packages up tests for that they can run outside of the build tree.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"


OUT_DIR=${DEFAULT_BUILD_ROOT}/x86/tests
mkdir -p ${OUT_DIR}
TARGET=${OUT_DIR}/cryptohome_tests

# package_tests target runfile
function package_tests() {
  local package="$1"
  local testfile="$2"
  shift; shift
  local libs="$@"
  TMPDIR="${TMPDIR:-/tmp}"
  local builddir="$(mktemp -d $TMPDIR/shpkgr.XXXXXX)"
  eval "cleanup() { [[ -n \"$builddir\" ]] && rm -rf $builddir; }"
  trap cleanup ERR

  cat <<-EOF > $package
	#!/bin/sh
	# Self extracting archive - reqs bash,test,rm,pwd,tar,gzip,tail,basename
	# Generated from "$0"
	# export PKG_LEAVE_RUNFILES=1 to keep the exploded archive.
	PREV=\`pwd\`
	test \$? -eq 0 || exit 1
	BASE=\`basename \$0\`
	test \$? -eq 0 || exit 1
	export RUNFILES=\`mktemp -d \$PREV/\$BASE.runfiles_XXXXXX\`
	test \$? -eq 0 || exit 1
	# delete the runfiles on exit using a trap
	trap "test \$PKG_LEAVE_RUNFILES || rm -rf \$RUNFILES" EXIT
	# extract starting at the last line (20)
	tail -n +20 \$0 | gzip -dc | tar x -C \$RUNFILES
	test \$? -eq 0 || exit 1
	# execute the package but keep the current directory
	/bin/bash --noprofile --norc -c "cd \$RUNFILES;. $testfile" \$0 "\$@"
	exit \$?
	__PACKAGER_TARBALL_GZ__
	EOF
  pushd $builddir &> /dev/null
  local source="$OLDPWD/$(dirname $0)"
  cp -r "$source/../../third_party/shunit2" shunit2
  cp -r "$source/lib" lib
  cp -r "$source/bin" bin
  cp -r "$source/tests" tests
  cp -a "$source/$testfile" $testfile
  tar --exclude=".svn" --exclude=".git" -czf - * >> $package
  popd &> /dev/null
  trap - ERR
  cleanup
  chmod +x $package
}

package_tests "$TARGET" test.sh
