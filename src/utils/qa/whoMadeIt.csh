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

# set up equivalence to remove split contributions under two names
set names=( 'Ann Zweig' 'Brian Raney' 'Brooke Rhead' 'Jim Kent' 'Andy Pohl'\
  'Larry Meyer' 'Mark Diekhans' 'Kate Rosenbloom' 'Hiram Clawson' 'Tim Dreszer' \
  'Galt Barber' 'Belinda Giardine' 'Angie Hinrichs' 'Robert Baertsch' )
set alias=( ann braney rhead kent aamp \
  larrym markd kate hiram tdreszer  \
  galt giardine angie baertsch )
# set names=( 'Jim Kent' )
# set alias=( kent )
set aliases=`echo $alias | wc -w`

if ( $#argv != 1 ) then
  echo
  echo "  gets info about who wrote the lines in a program."
  echo "  expects your source tree in your ~/kent directory."
  echo "  will work on a directory name."
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
if ( `file $location | awk '{print $NF}'` == "directory" ) then
  set full=""
  set files=`ls $location | egrep -v '.o$'`
  foreach file ( $files )
    set full="$full $location/$file"
  end
  set location="$full"
endif

foreach file ( $location )
  echo $file
  git blame $file | awk -F'(' '{print $2}' \
    | awk -F'20' '{print $1}' | sort \
    | uniq -c | sort -k2 -nr >& xxOutFilexx
  set size=`cat xxOutFilexx | awk '{total+=$1} END {print total}'`

  set i=$aliases
  mv xxOutFilexx tmp$i

  while ( $i > 0 )
    set first=`cat tmp$i | egrep "$alias[$i]" | awk '{print $1}'`
    set secon=`cat tmp$i | egrep "$names[$i]" | awk '{print $1}'`
    set sum=`echo $first $secon | awk '{print $1+$2}'`

    if ( $sum > $first ) then
      cat tmp$i | grep -v "${first} $alias[$i]" \
        | sed "/$names[$i]/ s/$secon/$sum/" \
        | sort -k1,1 -nr > temp
    else
      cat tmp$i | sed "s/$alias[$i]/$names[$i]/" > temp
    endif

    rm -f tmp$i
    @ i = $i - 1
    mv temp tmp$i
  end
  mv tmp0 xxOutFilexx

  cat xxOutFilexx | awk '{print $1, $2, $3}' | sed "s/ /_/" \
    | awk -F"_" '{printf("%7s %-10s\n", $1, $2 )}'
  if ( `wc -l xxOutFilexx | awk '{print $1}'` > 1 ) then
    echo "-----" "-----" | awk '{printf("%7s %-10s\n", $1, $2)}'
    echo $size   "total" | awk '{printf("%7s %-10s\n", $1, $2)}'
  endif 
  rm xxOutFilexx
  echo
end
echo
exit
