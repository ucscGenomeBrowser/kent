#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

set dir = "v"$BRANCHNN"_branch"

cd $BUILDDIR/$dir

cd kent
pwd
#
echo "Make libs."
cd src
make libs >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log | grep -v gbWarn`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make alpha."
cd hg
make alpha >& make.alpha.log
# These flags and programs will trip the error detection
sed -i -e "s/-DJK_WARN//g" make.alpha.log
sed -i -e "s/-Werror//g" make.alpha.log
sed -i -e "s/gbWarn\.//g" make.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
 echo "$res"
 exit 1
endif
#
# RUN vgGetText
echo "making vgGetText"
cd $BUILDDIR/$dir/kent/src/hg/visiGene/vgGetText
make alpha >& make.alpha.log
sed -i -e "s/-DJK_WARN//g" make.alpha.log
sed -i -e "s/-Werror//g" make.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
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
echo "Build libs, alpha, strict-track  done."
#
exit 0

