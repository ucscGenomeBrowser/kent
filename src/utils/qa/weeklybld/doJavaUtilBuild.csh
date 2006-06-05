#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doJavaUtilBuild.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev
# need to create a mini-sandbox to build these robot utilities
if ( -d $JAVABUILD ) then
  rm -fr $JAVABUILD
endif

mkdir $JAVABUILD
cd $JAVABUILD
cvs co -d . kent/java
if ( $status ) then
 echo "cvs check-out failed for kent/java to ${HOST}\:${JAVABUILD}"
 exit 1
endif

cd $JAVABUILD
build

echo "doJavaUtilBuild done."

exit 0

