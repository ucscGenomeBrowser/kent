#!/bin/tcsh
cd $WEEKLYBLD

set dir = "v"$BRANCHNN"_branch"

cd $BUILDDIR/$dir

cd kent/src
pwd
#
echo "Make libs. [${0}: `date`]"
make -j 32 libs >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log | grep -v gbWarn | grep -v bigWarn`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make beta. [${0}: `date`]"
make beta >& make.beta.log
# These flags and programs will trip the error detection
sed -i -e "s/-DJK_WARN//g" make.beta.log
sed -i -e "s/-Werror//g" make.beta.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.beta.log | /bin/grep -v gbWarn | grep -v bigWarn`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "beta errs found:"
 echo "$res"
 exit 1
endif
#
# RUN vgGetText
echo "making vgGetText [${0}: `date`]"
cd $BUILDDIR/$dir/kent/src/hg/visiGene/vgGetText
make beta >& make.beta.log
sed -i -e "s/-DJK_WARN//g" make.beta.log
sed -i -e "s/-Werror//g" make.beta.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.beta.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "beta errs found after vGetText:"
 echo "$res"
 exit 1
endif

# MAKE STRICT BETA TRACKDB
cd $WEEKLYBLD
./makeStrictBeta.csh
if ( $status ) then
 echo "trackDb make strict error."
 exit 1
endif
#
# Build Table Descriptions
#
# QA is now running the buildTableDescriptions.pl script on hgwbeta as
# a nightly cron job - Ann set this up  (2007-01-10)

#echo "buildTableDescriptions.pl."
#echo "see buildDescr.log for output"
#cd $BUILDDIR/$dir

# !!!! WARNING: AS IS MAY REQUIRE TWEAKING OF hg.conf TO WORK
#kent/src/test/buildTableDescriptions.pl -kentSrc kent/src -gbdDPath /usr/local/apache/htdocs/goldenPath/gbdDescriptions.html >& /cluster/bin/build/scripts/buildDescr.log
#set err = $status
#if ( $err ) then
#    echo "buildTableDescriptions.pl returned error $err"
#    exit 1
#endif
#
echo
echo "Build libs, alpha, strict-track  done. [${0}: `date`]"
#
exit 0

