#!/bin/tcsh
cd $WEEKLYBLD

# ------------------------------------
# Note - this script assumes you have your ssh key in
# qateam@hgdownload:.ssh/authorized_keys. Without it,
#  this script can NOT be launched from beta
#  using something like ssh $BOX32 $WEEKLYBLD/buildCgi32.csh
#  because when scp needs the password typed in, apparently
#  the stdin is not available from the terminal.
# Instead, log directly into box32 and execute the script.
#  then when prompted for the password, put in the qateam pwd. 
# ------------------------------------

if (("$HOST" != "$BOX32") && ("$HOST" != "hgwbeta")) then
 echo "error: you must run this script on $BOX32 or on hgwbeta!"
 exit 1
endif

set ScriptStart=`date`

echo "Cleaning out $BUILDDIR/liftOver"
rm -fr $BUILDDIR/liftOver
mkdir $BUILDDIR/liftOver

cd $BUILDDIR/liftOver

echo "Checking out lib part of branch $BRANCHNN. [${0}: `date`]"

# makefile make libs wants to make jkOwnLib and hg/protein which I don't care about
#cd ..
#cvs -d hgwdev:$CVSROOT co -d liftOver -r "v"$BRANCHNN"_branch"  kent/src/makefile #>& /dev/null
#set err = $status
#if ( $err ) then
# echo "error running cvs co kent/src/makefile in $BUILDDIR/liftOver : $err [${0}: `date`]" 
# exit 1
#endif
#cd liftOver

cvs -d hgwdev:$CVSROOT co -d inc -r "v"$BRANCHNN"_branch"  kent/src/inc >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent/src/inc in $BUILDDIR/liftOver : $err [${0}: `date`]" 
 exit 1
endif

cvs -d hgwdev:$CVSROOT co -d lib -r "v"$BRANCHNN"_branch"  kent/src/lib >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent/src/lib in $BUILDDIR/liftOver : $err [${0}: `date`]" 
 exit 1
endif

mkdir hg
cd hg

echo "Checking out hg/lib of branch $BRANCHNN. [${0}: `date`]"
cvs -d hgwdev:$CVSROOT co -d inc -r "v"$BRANCHNN"_branch"  kent/src/hg/inc >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent/src/hg/inc in $BUILDDIR/liftOver : $err [${0}: `date`]" 
 exit 1
endif

cvs -d hgwdev:$CVSROOT co -d lib -r "v"$BRANCHNN"_branch"  kent/src/hg/lib >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent/src/hg/lib in $BUILDDIR/liftOver : $err [${0}: `date`]" 
 exit 1
endif

echo "Checking out hg/liftOver of branch $BRANCHNN. [${0}: `date`]"
cvs -d hgwdev:$CVSROOT co -d liftOver -r "v"$BRANCHNN"_branch"  kent/src/hg/liftOver >& /dev/null
set err = $status
if ( $err ) then
 echo "error running cvs co kent/src/hg/liftOver in $BUILDDIR/liftOver : $err [${0}: `date`]" 
 exit 1
endif

cd ..

echo "making lib without ssl on $HOST [${0}: `date`]"

cd lib
make USE_SSL=0 >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log |grep -v "gbExtFile.o gbWarn.o gbMiscDiff.o"`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found on $HOST :  [${0}: `date`]"
 echo "$res"
 exit 1
endif
#
cd ..


echo "making hg/lib without ssl on $HOST [${0}: `date`]"

cd hg/lib
make USE_SSL=0 >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log |grep -v "gbWarn.o"`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found on $HOST :  [${0}: `date`]"
 echo "$res"
 exit 1
endif
#
cd ../..


echo "Make liftOver without ssl on $HOST. [${0}: `date`]"
cd hg/liftOver
make USE_SSL=0 BINDIR=. liftOver >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "make errs found on $HOST : [${0}: `date`]"
 echo "$res"
 exit 1
endif

scp -p liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/liftOver.linux.${MACHTYPE}
cd ../..

echo "liftOver without ssl $MACHTYPE built on $HOST and scp'd to hgdownload [${0}: START=${ScriptStart} END=`date`]"

exit 0

