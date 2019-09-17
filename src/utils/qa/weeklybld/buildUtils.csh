#!/bin/tcsh
# Build utils into /cluster/bin/$MACHTYPE on dev or beta from branch or tip
# specify "tip" as command line parm 1 to build from tip sandbox 

cd $WEEKLYBLD

set MAKEPARAMS=""

set branch=v${BRANCHNN}_branch

if ( "$1" == "tip" ) then
    set base=$BUILDDIR/tip
    echo "updating tip sandbox on $HOST [${0}: `date`]"
    cd $base/kent
    git pull -q origin
    echo "done updating tip sandbox"
    cd $WEEKLYBLD
else
    set base=$BUILDDIR/$branch
endif

if ( -d ~/bin/${MACHTYPE}.orig ) then
 echo "restoring from last failed symlink. on $HOST [${0}: `date`]"
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/${MACHTYPE}.cluster ) then
 echo "something messed up in symlink on $HOST [${0}: `date`]"
 exit 1
endif

# Hiram changed default script location
# which we must now over-ride.
# wonder if we should put this into 
# the sandbox config thing like we do for
# the SSH and BAM configs
setenv SCRIPTS /cluster/bin/scripts

# Symlink Trick safe now
echo "Symlink Trick. on $HOST [${0}: `date`]"
./symtrick.csh

echo
echo "Building src utils. on $HOST [${0}: `date`]"
cd $base/kent/src
echo "Before make utils on $HOST [${0}: `date`]"
    make -j 32 $MAKEPARAMS utils >& make.utils.log
# parasol is special since it was added to the userApps
cd $base/kent/src/parasol
    make -j 32 $MAKEPARAMS userApps >>& ../make.utils.log
cd $base/kent/src
echo "After make utils on $HOST [${0}: `date`]"
make $MAKEPARAMS blatSuite >>& make.utils.log
echo "After make blatSuite on $HOST [${0}: `date`]"
sed -i -e "s/-DJK_WARN//g" make.utils.log
sed -i -e "s/-Werror//g" make.utils.log
sed -i -e "s/gbWarn//g" make.utils.log
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.utils.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found on $HOST"
 echo "$res"
 $WEEKLYBLD/unsymtrick.csh
 exit 1
endif

# Undo Symlink trick
$WEEKLYBLD/unsymtrick.csh
echo "Restore: undoing Symlink Trick. on $HOST [${0}: `date`]"

echo
echo "Build of Utils on $HOST complete. [${0}: `date`]"
echo

exit 0

