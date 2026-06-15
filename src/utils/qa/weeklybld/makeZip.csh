#!/bin/tcsh
cd $BUILDDIR
set zip = "zips/jksrc.v"$BRANCHNN".zip"
if ( -e $zip ) then
 echo "removing old zip $zip [${0}: `date`]"
 rm $zip
endif

# git archive does not include submodule contents (e.g. src/submodules/htslib),
# so build the zip from a fresh clone of the branch with its submodules checked
# out, then strip the git metadata.
set tmp = "zips/jksrc.v"$BRANCHNN".tmp"
if ( -e $tmp ) then
 echo "removing old tmp clone $tmp [${0}: `date`]"
 rm -fr $tmp
endif
mkdir -p $tmp

echo "Cloning branch $BRANCHNN with submodules. [${0}: `date`]"
git clone -q --recurse-submodules --branch v${BRANCHNN}_branch $GITSHAREDREPO $tmp/kent
set err = $status
if ( $err ) then
 echo "error cloning branch v${BRANCHNN}_branch: $err [${0}: `date`]"
 rm -fr $tmp
 exit 1
endif

# htslib's htscodecs version.h is generated from git describe and is only built
# when a .git is present, so generate it now while the clone still has git.
# htscodecs.mk is a generated compiler-probe config that the build recreates, so
# drop it to keep the zip a clean source release.
set htslibDir = "$tmp/kent/src/submodules/htslib"
echo "Generating htslib version headers. [${0}: `date`]"
( cd $htslibDir && make version.h htscodecs/htscodecs/version.h )
set err = $status
if ( $err ) then
 echo "error generating htslib version headers: $err [${0}: `date`]"
 rm -fr $tmp
 exit 1
endif
rm -f $htslibDir/htscodecs.mk

# drop git metadata (superproject .git dir and submodule .git files) so the
# zip contains only source, matching the old git-archive output.
find $tmp/kent -name .git -prune -exec rm -rf {} +

echo "Dumping branch $BRANCHNN to zip file. [${0}: `date`]"
cd $tmp
zip -q -r -X $BUILDDIR/$zip kent
set err = $status
cd $BUILDDIR
if ( $err ) then
 echo "error creating zip $zip: $err [${0}: `date`]"
 rm -fr $tmp
 exit 1
endif

rm -fr $tmp
chmod 664 $zip
echo "Done. [${0}: `date`]"
exit 0
