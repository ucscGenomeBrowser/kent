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

onintr cleanup

set program=""
set location=""
set size=""

# set up equivalence to remove split contributions under two names
set names=( 'Ann Zweig' 'Brian Raney' 'Brooke Rhead' 'Jim Kent' 'Andy Pohl'\
  'Larry Meyer' 'Mark Diekhans' 'Kate Rosenbloom' 'Hiram Clawson' 'Tim Dreszer' \
  'Galt Barber' 'Belinda Giardine' 'Angie Hinrichs' 'Robert Baertsch' \
  'Donna Karolchik' 'Fan Hsu')
set alias=( ann braney rhead kent aamp \
  larrym markd kate hiram tdreszer  \
  galt giardine angie baertsch \
  donnak fanhsu)
# set names=( 'Jim Kent' )
# set alias=( kent )
set aliases=`echo $alias | wc -w`

if ( $#argv != 1 ) then
  echo
  echo "  gets info about who wrote how many lines in a program."
  echo "  expects your source tree in your ~/kent directory."
  echo "  will work on a directory name."
  echo "  writes some files in your kent directory, then removes them."
  echo
  echo "    usage:  `basename $0` program"
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
set isDir=0
if ( `file $location | awk '{print $NF}'` == "directory" ) then
  set isDir=1
  set full=""
  set files=`ls $location | egrep -v '.o$'`
  foreach file ( $files )
    set full="$full $location/$file"
  end
  set location="$full"
endif

rm -f grandTotFile$$
foreach file ( $location )
  echo $file
  git blame $file | awk -F'(' '{print $2}' \
    | awk -F'20' '{print $1}' | sort \
    | uniq -c | sort -k2 -nr >& xxOutFilexx
  set size=`cat xxOutFilexx | awk '{total+=$1} END {print total}'`

  set i=$aliases
  mv xxOutFilexx tmp001$i

  while ( $i > 0 )
    # sum and substitute to get one name per person
    set first=`cat tmp001$i | egrep "$alias[$i]" | awk '{print $1}'`
    set secon=`cat tmp001$i | egrep "$names[$i]" | awk '{print $1}'`
    set sum=`echo $first $secon | awk '{print $1+$2}'`

    if ( $sum > $first ) then
      cat tmp001$i | grep -v "${first} $alias[$i]" \
        | sed "/$names[$i]/ s/$secon/$sum/" \
        | sort -k1,1 -nr > temp$$
    else
      cat tmp001$i | sed "s/$alias[$i]/$names[$i]/" > temp$$
    endif

    rm -f tmp001$i
    @ i = $i - 1
    mv temp$$ tmp001$i
  end
  # last tmp001$i is tmp0010

  cat tmp0010 | awk '{print $1, $2, $3}' | sed "s/ /_/" \
    | awk -F"_" '{printf("%7s %-10s\n", $1, $2 )}'
  if ( `wc -l tmp0010 | awk '{print $1}'` > 1 ) then
    echo "-----" "-----" | awk '{printf("%7s %-10s\n", $1, $2)}'
    echo $size   "total" | awk '{printf("%7s %-10s\n", $1, $2)}'
  endif

  if ( $isDir == 1 ) then
    cat tmp0010 >> grandTotFile$$
    rm -f tmp0010
  endif
  echo
end

if ( $isDir == 1 ) then
  echo "Total for $program directory"
  echo "-----------------------------"
  echo
else
  exit
endif

if ( -e grandTotFile$$ ) then
  # get list of names
  set nameList=`cat grandTotFile$$ | awk '{print $2"_"$3}' | sort -u`
  # echo nameList $nameList
  rm -f finFile$$
  foreach person ( $nameList )
    set name=`echo $person | sed "s/_/ /"`
    cat grandTotFile$$ | grep "$name" \
      | awk '{total+=$1} END {print total, $2, $3}' >> finFile$$
  end
  set totsize=`cat finFile | grep -v "Not Committed" \
    | awk '{total+=$1} END {print total, $2, $3}'`
  cat finFile$$ | grep -v "Not Committed" | sort -nr \
    | awk '{print $1, $2, $3}' | sed "s/ /_/" \
    | awk -F"_" '{printf("%7s %-10s\n", $1, $2 )}'
  echo "-----" "-----"  | awk '{printf("%7s %-10s\n", $1, $2)}'
  echo $totsize         | awk '{printf("%7s\n", $1)}'
endif

rm -f grandTotFile$$
rm -f finFile$$
echo
exit

cleanup:
rm -f temp$$
rm -f xxOutFilexx
rm -f tmp001$i
rm -f tmp0010
rm -f finFile$$
