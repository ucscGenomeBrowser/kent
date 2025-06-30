#!/bin/tcsh
# ------------------------------------
# Note - this script assumes you have your ssh key in
# qateam@hgdownload:.ssh/authorized_keys. Without it,
#  this script can NOT be launched from dev
# ------------------------------------

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
git checkout --track=direct -b $branch origin/$branch
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

# change this to false to use local make rather than Docker
set useDocker=true

cd kent/src
if ("$useDocker" == "true") then
   $WEEKLYBLD/userAppsCompileInDocker $BUILDDIR > make.log
else
   make -j 12 BINDIR=$BINDIR DESTDIR=$DESTDIR userApps > make.log
endif
  
./utils/userApps/mkREADME.sh $DESTDIR$BINDIR FOOTER.txt
cd ../..

# copy everything if 64 bit
if ("$HOST" == "hgwdev") then
  #clear out the old and copy in the new
  foreach f ( ${DESTDIR}${BINDIR}/* )
    echo $f
    switch ($f)
	# these three go in the blat subdirectory
	case ${DESTDIR}${BINDIR}/blat:
	case ${DESTDIR}${BINDIR}/blatHuge:
	case ${DESTDIR}${BINDIR}/gfClient:
	case ${DESTDIR}${BINDIR}/gfServer:
	case ${DESTDIR}${BINDIR}/gfServerHuge:
	case ${DESTDIR}${BINDIR}/gfPcr:
	case ${DESTDIR}${BINDIR}/isPcr:
	    ssh -n qateam@hgdownload "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
#	    ssh -n qateam@hgdownload2 "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
#	    scp -p $f qateam@hgdownload2:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    ssh -n qateam@hgdownload3 "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@hgdownload3:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    ssh -n qateam@genome-euro "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@genome-euro:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    breaksw
	default:
	    ssh -n qateam@hgdownload "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
#	    ssh -n qateam@hgdownload2 "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
#	    scp -p $f qateam@hgdownload2:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    ssh -n qateam@hgdownload3 "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@hgdownload3:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    ssh -n qateam@genome-euro "rm -f /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@genome-euro:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    breaksw
    endsw
  end
endif

# because blat/FOOTER.txt is not updated automatically, 
# on rare occasions when you add new blat utilities, you can do it manually,
# refer to NOTES-for-manually-updating-BLAT-FOOTER.txt

echo "userApps $MACHTYPE built on $HOST and scp'd to hgdownload and genome-euro [${0}: START=${ScriptStart} END=`date`]"

exit 0

