#!/bin/tcsh

##############################
#
# 1-20-2006
# This script does the triple comm command 
# to compare the diffs between two files
#
# comm -23 left right > left.Only
# comm -13 left right > right.Only
# comm -12 left right > left.right.Only
# wc -l *Only | grep -v otal
#
################################

set left=""
set right=""
set leftFile=""
set rightFile=""
set leftDir=""
set rightDir=""


# useage statement
if ($#argv < 2 || $#argv > 3) then
  echo
  echo " Sorts and compares two files.  "
  echo " Counts unique and common records."
  echo
  echo "     usage:  leftFileName rightFileName [rm]"
  echo "             optional [rm]: remove the three output files when finished"
  echo
  exit 1
endif

# strip the directory and file name off the user's input
# these variables hold the path/filename as the user input them
set left=$1
set right=$2

# ":t" takes just the 'tail' of the file input (i.e. FileName.ext)
set leftFile=$1:t
set rightFile=$2:t

# ":h" takes just the 'path' of the file input (i.e. directory structure)
set leftDir=$1:h
set rightDir=$2:h


# make sure the files exist
if (! -e $left) then
  echo "sorry, can't find your file named $left - quitting now"
  echo
  exit 1
endif

if (! -e $right) then
  echo "sorry, can't find your file named $right - quitting now"
  echo
  exit 1
endif

# sort the files first

sort $left > leftSort
sort $right > rightSort
 

# now do it

comm -23 leftSort rightSort > $left.Only
comm -13 leftSort rightSort > $right.Only
comm -12 leftSort rightSort > $left.$rightFile.Only

wc -l $left.Only $right.Only $left.$rightFile.Only | grep -v otal

echo

# remove the intermediate files

rm leftSort
rm rightSort


# remove the output files if they asked for it
if ($#argv == 3) then
  if ($argv[3] == "RM" || $argv[3] == "rm") then
    rm -f $left.Only
    rm -f $right.Only
    rm -f $left.$rightFile.Only
    echo "removed output files"
    echo
    exit
  endif
endif


