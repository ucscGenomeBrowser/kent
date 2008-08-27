#!/bin/tcsh
# Build utils into /cluster/bin/$MACHTYPE on dev or beta from branch or tip
# specify "tip" as command line parm 1 to build from tip sandbox 

cd $WEEKLYBLD

set MAKEPARAMS=""
if ( "$MACHTYPE" == "i386" ) then
    if ( "$HOST" != "$BOX32" ) then
	echo "error: you must run this script on $BOX32!"
	exit 1
    endif
    # hack to force use of gcc34 on titan rather than the newer gcc (4.1) which makes binaries
    # that can't run on our x86_4 machines.
    set MAKEPARAMS="CC=gcc34"
endif
if ( "$MACHTYPE" == "x86_64" ) then
    if ( "$HOST" != "hgwbeta" ) then
	echo "error: you must run this script on hgwbeta!"
	exit 1
    endif
endif

set branch=v${BRANCHNN}_branch

if ( "$1" == "tip" ) then
    set base=$BUILDDIR/tip
    echo "updating tip sandbox"
    cd $base/kent
    cvs up -dP  >& /dev/null
    echo "done updating tip sandbox"
    cd $WEEKLYBLD
else
    set base=$BUILDDIR/$branch
endif

if ( -d ~/bin/${MACHTYPE}.orig ) then
 echo "restoring from last failed symlink."
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/${MACHTYPE}.cluster ) then
 echo "something messed up in symlink"
 exit 1
endif

# Symlink Trick safe now
echo "Symlink Trick."
./symtrick.csh

echo
echo "Building src utils."
cd $base/kent/src
echo "Before make utils"
make $MAKEPARAMS utils >& make.utils.log
echo "After make utils"
make $MAKEPARAMS blatSuite >>& make.utils.log
echo "After make blatSuite"
sed -i -e "s/-DJK_WARN//g" make.utils.log
sed -i -e "s/-Werror//g" make.utils.log
sed -i -e "s/gbWarn//g" make.utils.log
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.utils.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 $WEEKLYBLD/unsymtrick.csh
 exit 1
endif

# Undo Symlink trick
$WEEKLYBLD/unsymtrick.csh
echo "Restore: undoing Symlink Trick."

echo
echo "Build of Utils on $HOST complete."
echo

exit 0

