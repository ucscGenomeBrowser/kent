#!/bin/tcsh
cd $WEEKLYBLD

# for cron job
#source .cshrc
#setenv SHELL /bin/tcsh
#setenv PATH /bin:/usr/bin:/usr/X11R6/bin:/usr/local/bin:/cluster/bin/i386
#setenv HOSTNAME $HOST

if ( "$HOST" != "kolossus" ) then
 echo "error: you must run this script on kolossus!"
 exit 1
endif

cd $BUILDDIR
set dir = "v"$BRANCHNN"_branch"
cd $dir

cd kent
pwd
#
echo "Make clean."
cd src
make clean >& make.64.clean.log
echo "Make libs."
make libs >& make.64.log
sed -i -e "s/-DJK_WARN//" make.64.log
sed -i -e "s/-Werror//" make.64.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.64.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "libs errs found:"
 echo "$res"
 exit 1
endif
#
echo "Make alpha."
cd hg
make alpha >& make.64.alpha.log
sed -i -e "s/-DJK_WARN//" make.64.alpha.log
sed -i -e "s/-Werror//" make.64.alpha.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.64.alpha.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "alpha errs found:"
 echo "$res"
 exit 1
endif
#
cd $WEEKLYBLD
./makeStrictKolossus.csh
if ( $status ) then
 echo "trackDb make strict error."
 exit 1
endif
#
echo "buildTableDescriptions.pl."
#
cd $BUILDDIR/$dir
kent/src/test/buildTableDescriptions.pl -kentSrc kent/src -gbdDPath /usr/local/apache/htdocs/goldenPath/gbdDescriptions.html >& buildDescr.64.log
echo see buildDescr.64.log for output
#
echo
echo "Build libs, alpha, strict-track/zoo, table-descr  done."
#
exit 0

