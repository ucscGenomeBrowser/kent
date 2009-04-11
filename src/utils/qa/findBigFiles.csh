#!/bin/tcsh
source `which qaConfig.csh`

if ($#argv != 1) then
  # no command line args
  echo
  echo "  gets the sizes of files from here down sorted by size."
  echo
  echo "    usage:  go"
  echo
  exit
else
  if ($argv[1] != "go") then
    $0 
    exit 1
  endif
endif

find . -type f | xargs ls -og | awk '{print $3, "\t", $7}' | sort -n \
  | tail -50
