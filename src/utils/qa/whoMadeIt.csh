#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  02-22-07
#  Robert Kuhn
#
#  gets info about who wrote the lines in a program
#
################################

set program=""
set location=""
set size=""

if ( $#argv != 1 ) then
  echo
  echo "  gets info about who wrote the lines in a program."
  echo
  echo "    usage:  program"
  echo
  exit
else
  set program=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

cd ~/kent
set location=`find . -name $program`
echo

# find out if input location is a directory
# and prepend the path to each filename
# omitting dot oh files
git blame $location >& /dev/null
if ( $status == 128 ) then
  set full=""
  set files=`ls $location | egrep -v '.o$'`
  foreach file ( $files )
    set full="`echo $full` $location/$file"
  end
  set location=`echo $full`
endif

foreach file ( $location )
  echo $file
  git blame $file | awk -F'(' '{print $2}' \
    | awk -F'20' '{print $1}' | sort \
    | uniq -c | sort -k2 -nr >& xxOutFilexx
  set size=`cat xxOutFilexx | awk '{total+=$1} END {print total}'`
  cat xxOutFilexx
  if ( `wc -l xxOutFilexx | awk '{print $1}'` > 1 ) then
    echo "-----" "-----" | awk '{printf("%7s %-10s\n", $1, $2)}'
    echo $size "total" | awk '{printf("%7s %-10s\n", $1, $2)}'
  endif 
  rm xxOutFilexx
  echo
end

echo

