#!/bin/tcsh -efx

#
# Testing suggestion, as follows.  For all files created today (this
# assumes that you're testing on the same day you ran the code), compare
# the size of the files created to the size of the equivalent files created
# in the previous version.  Adjust ctime to do this test for work performed
# on a different day.
#
set ctimeArg = -1
#if ($#argv > 4) then
#    set ctimeArg = $5
#endif
find . -type f -ctime $ctimeArg -print > $1/modifiedFiles.txt
cat $1/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $1/$2 
pushd $3
cat $1/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $1/$4 
popd
sdiff $1/$2 $1/$4

