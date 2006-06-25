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
sed -i -e "s/-DJK_WARN//" make.log
sed -i -e "s/-Werror//" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log`
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
sed -i -e "s/-DJK_WARN//" make.alpha.log
sed -i -e "s/-Werror//" make.alpha.log
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
cd $BUILDDIR/$dir/kent/hg/visiGene/vgGetText
make alpha >& make.alpha.log
sed -i -e "s/-DJK_WARN//" make.alpha.log
sed -i -e "s/-Werror//" make.alpha.log
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
echo "buildTableDescriptions.pl."
#
cd $BUILDDIR/$dir
kent/src/test/buildTableDescriptions.pl -kentSrc kent/src -gbdDPath /usr/local/apache/htdocs/goldenPath/gbdDescriptions.html >& buildDescr.log
echo see buildDescr.log for output
#
echo
echo "Build libs, alpha, strict-track/zoo, table-descr  done."
#
exit 0

