#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doJavaUtilBuild.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev
# need to create a mini-sandbox to build these robot utilities
if ( -d $JAVABUILD ) then
  rm -fr $JAVABUILD/*
endif

if (! -d $JAVABUILD ) then
    mkdir $JAVABUILD
endif    
cd $JAVABUILD

git clone -q $GITSHAREDREPO kent
cd kent
git checkout -tb $dir origin/$dir
set err = $status
if ( $err ) then
 echo "error running git clone and checkout of kent in $BUILDDIR/$dir : $err [${0}: `date`]" 
 exit 1
endif 
cd ..

cd kent/java
./build

echo "doJavaUtilBuild done."

exit 0

