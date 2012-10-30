#!/bin/tcsh
cd $WEEKLYBLD

# ------------------------------------
# Note - this script assumes you have your ssh key in
# qateam@hgdownload:.ssh/authorized_keys. Without it,
#  this script can NOT be launched from beta
#9403# #  using something like ssh $BOX32 $WEEKLYBLD/buildCgi32.csh
#9403# #  because when scp needs the password typed in, apparently
#9403# #  the stdin is not available from the terminal.
#9403# # Instead, log directly into box32 and execute the script.
#9403# #  then when prompted for the password, put in the qateam pwd. 
# ------------------------------------

#9403# if (("$HOST" != "$BOX32") && ("$HOST" != "hgwbeta")) then
#9403#  echo "error: you must run this script on $BOX32 or on hgwbeta!"
if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"

 exit 1
endif

set ScriptStart=`date`

echo "Cleaning out $BUILDDIR/userApps"
rm -fr $BUILDDIR/userApps
mkdir $BUILDDIR/userApps

cd $BUILDDIR/userApps

echo "Checking out kent/src branch $BRANCHNN. [${0}: `date`]"

set branch = "v"$BRANCHNN"_branch"

git clone -q $GITSHAREDREPO kent
chmod g+w kent
cd kent
git checkout -tb $branch origin/$branch
set err = $status
if ( $err ) then
 echo "error running git clone and checkout of kent in $BUILDDIR/userApps : $err [${0}: `date`]" 
 exit 1
endif 
cd ..

set BINDIR=linux.$MACHTYPE
set DESTDIR=$BUILDDIR/userApps/  # must end in slash because of makefile weirdness
rm -rf $DESTDIR$BINDIR
mkdir $DESTDIR$BINDIR

# configure settings like SSL and BAM in common.mk
echo "Configuring settings on userApp sandbox $BRANCHNN $HOST [${0}: `date`]"
$WEEKLYBLD/configureSandbox.csh . $WEEKLYBLD/downloadBuildSettings.mk

cd kent/src 
make BINDIR=$BINDIR DESTDIR=$DESTDIR userApps > make.log
./utils/userApps/mkREADME.sh $DESTDIR$BINDIR FOOTER
cd ../..

# copy everything if 64 bit
if ("$HOST" == "hgwbeta") then
  #clear out the old and copy in the new
  foreach f ( ${DESTDIR}${BINDIR}/* )
    echo $f
    switch ($f)
	# these three go in the blat subdirectory
	case ${DESTDIR}${BINDIR}/blat:
	case ${DESTDIR}${BINDIR}/gfClient:
	case ${DESTDIR}${BINDIR}/gfServer:
	    ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    breaksw
	default:
	    ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    breaksw
    endsw
  end
endif

#9403# # copy liftOver if 32 bit
#9403# if ("$HOST" == "$BOX32") then
#9403#   ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/liftOver"
#9403#   scp -p ${DESTDIR}${BINDIR}/liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/
#9403#   ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/liftOver"
#9403#   scp -p ${DESTDIR}${BINDIR}/liftOver qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/exe/$BINDIR/
#9403# endif

echo "userApps $MACHTYPE built on $HOST and scp'd to hgdownload and hgdownload-sd [${0}: START=${ScriptStart} END=`date`]"

exit 0

