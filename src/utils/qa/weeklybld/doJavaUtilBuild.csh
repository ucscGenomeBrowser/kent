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

set branch="v${BRANCHNN}_branch"

git clone -q $GITSHAREDREPO kent
chmod g+w kent
cd kent
git checkout -tb $branch origin/$branch
set err = $status
if ( $err ) then
 echo "error running git clone and checkout of kent in $JAVABUILD : $err [${0}: `date`]" 
 exit 1
endif 
cd ..

cd kent/java
./build

echo "doJavaUtilBuild done."

exit 0

