#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: dozip.csh must be executed from hgwdev!"
 exit 1
endif

ssh kkstore $WEEKLYBLD/makezip.csh
set err = $status
if ( $err ) then
 echo "error running makezip.csh: $err" 
 exit 1
endif 
ssh hgwbeta $WEEKLYBLD/buildzip.csh
set err = $status
if ( $err ) then
 echo "error running buildzip.csh: $err" 
 exit 1
endif
cp $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" /usr/local/apache/htdocs/admin/jksrc.zip
echo
echo "Zip Done. Make Push Request:"
echo
echo "Please push hgwdev --> hgdownload, "
echo "   /usr/local/apache/htdocs/admin/jksrc.zip"
echo "Thanks!"
echo
exit 0

