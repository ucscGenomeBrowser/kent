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

# ping the machines to load all the mounts
set j=12
while ( $j )
  ls /cluster/store$j >& /dev/null
  set j=`echo $j | awk '{print $1-1}'`
end
ls /cluster/bluearc >& /dev/null
ls /cluster/home >& /dev/null

rm -f storefile
df -h | egrep "store|bluearc|home|data|bin" \
  | sed -e "s/10.1.1.3:\/bluearc/                 /" \
  | sed -e "s/\/dev\/sdc1/         /" \
  | sed -e "s/\/dev\/sdd1/         /" \
  | sed -e "s/\/dev\/dm-8/         /" \
  | sed -e "s/\/dev\/dm-9/         /" \
  | sed -e "s/\/dev\/sde1/         /" \
  | egrep % | sort -k4 -r >> storefile
set fullunit=`awk '$4 == "100%" || $4 == "99%" || $4 == "98%" {print $5}' \
  storefile`

# display status of all units
echo
cat storefile
echo
rm storefile

#set number of units that need full du
set number=`echo $fullunit | awk -F" " '{print NF}'`

if ($number == 0) then
  exit
else
  set j=0
  while ($number - $j)
    # get them in order, most full unit first
    set j=`echo $j | awk '{print $1 + 1}'`
    set unit=$fullunit[$j]
    set storeName=`echo $unit | awk -F/ '{print $NF}'`
    set machine=`df | grep export$unit | awk -F- '{print $1}'`
    if (-e $unit/du.$date) then
      # don't run du again. simply display current totals
      echo "full du list at $unit/du.$date\n" 
      continue
    endif
    echo "sizes in megabytes:\n"
    echo "$unit\n"

    # get disk usage 4 dirs deep and sort by size
    # if the unit can't be logged into, do the du through the wire
    #   (ok per erich if only now and then)

    # ping the unit to be sure it is mounted
    ls $unit >& /dev/null
    if ( $unit =~ {bluearc,home} ) then
      du -m --max-depth=4 $unit | sort -nr > tempfile
    else
      # use ssh to do the du locally
      ssh $machine du -m --max-depth=4 $unit | sort -nr > tempfile
    endif
    # when du value is the same, "-k2,2" flag puts subdir second
    sort -k1,1nr -k2,2 tempfile  > du.$storeName.$date.temp
    rm -f tempfile

    # remove lines that have subdir on next line
    set n=0
    set fullfile="du.$storeName.$date.temp"
    set rows=`wc -l $fullfile | awk '{print $1-1}'`
    rm -f outfile
    set sizeN=101

    # keep info for files/dirs > 100 Mbytes only
    while ($rows - $n > 1 && $sizeN > 99)
  
      # get directory name and size about this and next row
      set n=`echo $n | awk '{print $1+1}'`
      set m=`echo $n | awk '{print $1+1}'`
      set dirN=`sed -n -e "${n}p" $fullfile | awk '{print $2}'`
      set dirM=`sed -n -e "${m}p" $fullfile | awk '{print $2}'`
      set sizeN=`sed -n -e "${n}p" $fullfile | awk '{print $1}'`
      set sizeN=`sed -n -e "${m}p" $fullfile | awk '{print $1}'`
  
      # check to see if next row is a subdir of present row.
      #   and drop present row if so.
      echo $dirM | grep $dirN > /dev/null
      if ($status) then
        # echo "keeping $dirN"
        # add names of file owners
        set ownerN=`ls -ld $dirN | awk '{print $3}'`
        echo "$sizeN	$ownerN	$dirN" >> outfile
      else
        # echo "dropping $dirN"
      endif
    end

    # copy output to the storeUnit itself
    cp outfile $unit/du.$date
    cp outfile du.$storeName.$date
    echo "full du list at $unit/du.$date\n" 
    head -40 outfile
    echo
    rm -f outfile du.$storeName.$date.temp
  end
endif

exit
