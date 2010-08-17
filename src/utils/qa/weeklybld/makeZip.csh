#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: makeZip.csh must be executed from hgwbeta!"
 exit 1
endif
cd $BUILDDIR
set zip = "zips/jksrc.v"$BRANCHNN".zip"
if ( -e $zip ) then
 echo "removing old zip $zip [${0}: `date`]"
 rm $zip
endif
echo "Dumping branch $BRANCHNN to zip file. [${0}: `date`]"
git archive --format=zip --prefix=kent/ --remote=$GITSHAREDREPO v${BRANCHNN}_branch > $zip
chmod 664 $zip
echo "Done. [${0}: `date`]"
exit 0
