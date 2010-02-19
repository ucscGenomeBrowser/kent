#!/bin/tcsh

################################
#  04-21-04
#  searches directory and prints out the usage page from .csh files.
#
################################


set currDir=$cwd

if ($1 == "") then
  # no command line args
  echo
  echo "  searches directory and prints out the usage page from .csh files."
  echo
  echo "    usage:  <bin | here> "
  echo
  exit
endif

if ($1 == "bin") then
  cd  /cluster/home/kuhn/bin
else
  if ($1 != "here") then
    echo
    echo "  searches directory and prints out the usage page from .csh files."
    echo
    echo "    usage:  <bin | here> "
    echo
    exit
  endif
endif

# set list=`wc -l dump | awk '{print $1}' | grep -v ls`
# set list=`wc -l dump | awk '{print $1}' | grep csh`
set list=`ls -l *.csh | grep csh | wc -l`
# echo "list = $list"

if ($list == 0) then
  echo
  echo "  sorry, no *.csh file in directory"
  echo
else
  foreach file (`ls *.csh | grep -v printMan.csh`)
    echo "--------------------------------------------"
    echo $file
    ./$file
  end
  echo "--------------------------------------------"
endif


if ($1 == "bin") then
  cd $currDir 
endif

