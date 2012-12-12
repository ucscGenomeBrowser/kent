#!/bin/tcsh
source `which qaConfig.csh`

if ( $#argv != 1 ) then
  # no command line args
  echo
  echo "  gets the sizes of files sorted by size."
  echo
  echo "    usage:  go | ."
  echo
  echo '       where "go" does the whole tree from here down.'
  echo '         and  "." does the current dir only'
  echo
  exit
endif

if ( $argv[1] == "go" ) then
  find . -type f | xargs ls -og | awk '{print $3, "\t", $7}' | sort -n \
    | tail -50
  exit
endif

if ( $argv[1] == "." ) then
  ls -og | awk '{print $3, "\t", $7}' | sort -n \
    | tail -50
  exit
endif

$0
exit 1
