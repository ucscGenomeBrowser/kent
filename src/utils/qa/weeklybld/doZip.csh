#!/bin/tcsh
cd $WEEKLYBLD

echo "Make zip [${0}: `date`]"
./makeZip.csh
set err = $status
if ( $err ) then
    echo "error running makezip.csh: $err [${0}: `date`]" 
    exit 1
endif 
./buildZip.csh
set err = $status
if ( $err ) then
    echo "error running buildzip.csh: $err [${0}: `date`]"  
    exit 1
endif

echo "removing old jksrc zip and symlink [${0}: `date`]"
ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
### ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
ssh -n qateam@genome-euro "rm /mirrordata/apache/htdocs/admin/jksrc.zip"

echo "scp-ing jksrc.v${BRANCHNN}.zip to hgdownload(s) [${0}: `date`]"
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload:/mirrordata/apache/htdocs/admin/
### scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@genome-euro:/mirrordata/apache/htdocs/admin/

echo "updating jksrc.zip symlink [${0}: `date`]"
ssh -n qateam@hgdownload "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"
### ssh -n qateam@hgdownload-sd "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"
ssh -n qateam@genome-euro "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"

# hgdocs/js/*.js files are not apparently needed on hgdownload as there is no directory there
#echo "scp-ing js/*.js files to hgdownload [${0}: `date`]"
#scp -p $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/js/*.js qateam@hgdownload:/mirrordata/apache/htdocs/js
#scp -p $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/js/*.js qateam@hgdownload-sd:/mirrordata/apache/htdocs/js
echo "Done. [${0}: `date`]"
