#!/bin/tcsh

################################
#
#  01-17-2012
#  Robert Kuhn
#
#  see the last change made in a git-repo'd file
#
################################


if ( $#argv != 1 ) then
  echo
  echo "  shows the diff for the last git checkin for a file"
  echo "     usage: showLastGit.csh filename"
  echo
  exit
else
  set file=$argv[1]
endif

set hash=`git log -1 $file | head -1 | awk '{print $2}'`
git show $hash $file

