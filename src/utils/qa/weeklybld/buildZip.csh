#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
    echo "error: makezip.csh must be executed from hgwbeta!"
    exit 1
endif

cd $BUILDDIR
cd zips
if ( -e kent ) then
    echo "cleaning out old zips/kent. [${0}: `date`]"
    rm -fr kent
endif

set zip = "jksrc.v"$BRANCHNN".zip"
echo "unzipping $zip. [${0}: `date`]"
unzip $zip >& /dev/null
set err = $status
if ( $err ) then
    echo "error unzipping $zip: $err [${0}: `date`]" 
    exit 1
endif 

echo "Make libs. [${0}: `date`]"
cd kent
cd src
make libs >& make.log
sed -i -e "s/-DJK_WARN//g" make.log
sed -i -e "s/-Werror//g" make.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.log | grep -v gbWarn`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
    echo "errs found: [${0}: `date`]"
    echo "$res"
    exit 1
endif
#
echo "Make compile. [${0}: `date`]"
cd hg
make compile >& make.compile.log
sed -i -e "s/-DJK_WARN//g" make.compile.log
sed -i -e "s/-Werror//g" make.compile.log
#-- report any compiler warnings, fix any errors (shouldn't be any)
#-- to check for errors: 
set res = `/bin/egrep -i "error|warn" make.compile.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
    echo "errs found: [${0}: `date`]"
    echo "$res"
    exit 1
endif
#
# clean up temp kent
cd $BUILDDIR
cd zips
if ( -e kent ) then
    echo "cleaning up temp kent dir in zips. [${0}: `date`]"
    rm -fr kent
endif

echo "Build libs, cgi done. [${0}: `date`]"
exit 0

