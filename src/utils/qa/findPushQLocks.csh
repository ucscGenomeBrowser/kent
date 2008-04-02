#!/bin/tcsh

######################################
#
# Ann Zweig -- 3-20-2008
#
# find locks in the pushQ on hgwbeta
# run with 'real' to unlock them all
#
######################################

set unlock=''

if ($#argv != 1 ) then
  echo
  echo "  find all locks in the pushQ on hgwbeta"
  echo
  echo "    usage: go | real"
  echo "     run with 'go' to see a list of locks"
  echo "     run with 'real' to unlock all the locks"
  echo
  exit
else
  set run=$argv[1]
endif

if ( 'go' == $run ) then
  hgsql -h hgwbeta -e "SELECT qid, lockUser, lockDateTime FROM pushQ \
  WHERE lockDateTime != '' or lockUser != ''" qapushq
else 
  set unlock=`hgsql -h hgwbeta -Ne "SELECT qid FROM pushQ \
  WHERE lockDateTime != '' or lockUser != ''" qapushq`
  if ( '' != $unlock ) then
    foreach lock ( $unlock )
      hgsql -h hgwbeta -e "UPDATE pushQ SET lockUser = '', lockDateTime = '' \
      WHERE qid = '$lock'" qapushq
      echo "\nunlocking qid: $lock"
    end
  else
    echo "\nno locks to unlock\n"
  endif
endif

exit 0
