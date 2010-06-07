#!/bin/tcsh
source `which qaConfig.csh`

####################
#  05-28-04 Bob Kuhn
#
#  Script to remove empty files from a directory
#
####################


set track=""
set refTrack=""
set db=""
set current_dir = $cwd
set uniqFlag=0

if ($#argv == 0) then
  # not enough command line args
  echo
  echo "  removes empty files from a directory."
  echo
  echo "    usage:  < . | path>"
  echo
  exit
else
  set filepath=$argv[1]
endif

# -------------------------------------------------
# get files:

# set files=`ls -og $filepath | gawk '{print $3, $7}' | grep " 0 "`
#  | gawk '{print $7}'`

set files=`ls -1sL $filepath | grep " 0 " | gawk '{print $2}'`

echo
echo "removing empty files:\n"
foreach file ($files)
  echo $file
  rm $file
end

# ls -1tr $filepath
echo
# echo "  most recent last"
# echo
