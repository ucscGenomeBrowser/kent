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
	case ${DESTDIR}${BINDIR}/gfClient:
	case ${DESTDIR}${BINDIR}/gfServer:
	    ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
###	    ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
###	    scp -p $f qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    ssh -n qateam@genome-euro "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t"
	    scp -p $f qateam@genome-euro:/mirrordata/apache/htdocs/admin/exe/$BINDIR/blat/$f:t
	    breaksw
	default:
	    ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
###	    ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
###	    scp -p $f qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    ssh -n qateam@genome-euro "rm /mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t"
	    scp -p $f qateam@genome-euro:/mirrordata/apache/htdocs/admin/exe/$BINDIR/$f:t
	    breaksw
    endsw
  end
endif

echo "userApps $MACHTYPE built on $HOST and scp'd to hgdownload, hgdownload-sd and genome-euro [${0}: START=${ScriptStart} END=`date`]"

exit 0

