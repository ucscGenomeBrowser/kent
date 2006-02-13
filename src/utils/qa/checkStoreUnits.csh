#!/bin/tcsh

#######################
#
#  08-06-05
#  checks all /cluster/store units and reports space avail
#
#######################

set date=`date +%Y%m%d`
set storeName=""
set machine=""
set number=0
set fullunit=""

if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

set machine=""
if ($#argv < 1 || $#argv > 1) then
  echo
  echo "  checks all /cluster/store units and reports space avail."
  echo
  echo "    usage:  go"
  echo
  exit
endif

rm -f storefile
df -kh | egrep "store|bluearc|home" \
  | sed -e "s/10.1.1.3:\/bluearc/                 /" \
  | egrep % | sort -k4 -r >> storefile
set fullunit=`awk '$4 == "100%" || $4 == "99%" || $4 == "98%" {print $5}' \
  storefile`

echo
cat storefile
echo
rm storefile


set number=`echo $fullunit | awk -F" " '{print NF}'`
if ($number == 0) then
  exit
else
  echo "sizes in megabytes:\n"
  set n=0
  while ($number - $n)
    # get them in order, most full unit first
    set n=`echo $n | awk '{print $1 + 1}'`
    set unit=$fullunit[$n]
    echo "$unit\n"
    set storeName=`echo $unit | awk -F/ '{print $NF}'`
    if (-e du.$storeName.$date) then
      # don't run du again. simply display current totals
      continue
    endif
    set machine=`df | grep export$unit | awk -F- '{print $1}'`
    ssh $machine du -m --max-depth=4 $unit | sort -nr \
      > du.$storeName.$date

    # add names of file owners
    cat du.$storeName.$date | awk '{print $1}' > sizeFile
    rm -f nameFile
    foreach file (`cat du.$storeName.$date | awk '{print $2}'`) 
       ls -ld $file | awk '{print $3 "\t" $9}' >> nameFile
    end
    paste sizeFile nameFile >  outFile

    # put output on the storeUnit itself
    cp outFile $unit/du.$date
    cp outFile du.$storeName.$date
    echo "full du list at $unit/du.$date\n" 
    head -40 outFile
    echo
    # rm -f sizeFile nameFile outFile
  end
endif

exit
