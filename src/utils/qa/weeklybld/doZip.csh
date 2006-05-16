#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
    echo "error: dozip.csh must be executed from hgwbeta!"
    exit 1
endif

./makeZip.csh
set err = $status
if ( $err ) then
    echo "error running makezip.csh: $err" 
    exit 1
endif 
./buildZip.csh
set err = $status
if ( $err ) then
    echo "error running buildzip.csh: $err" 
    exit 1
endif
cp $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" /usr/local/apache/htdocs/admin/jksrc.zip
echo
echo "Zip Done. Make Push Request:"
echo
echo "Please push hgwbeta --> hgdownload, "
echo "   /usr/local/apache/htdocs/admin/jksrc.zip"
echo "Thanks!"
echo
exit 0

