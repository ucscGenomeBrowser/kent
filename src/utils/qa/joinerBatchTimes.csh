#!/bin/tcsh 

##############################
#
# does joinerCheck -times for a set a databases at a time
#
##############################

set dbs = ''
set active = ''
set thisRun = ''

# set email name of person to whom email about this run gets delivered:
set who = "ann"

# usage statement
if ($#argv < 1 || $#argv > 2) then
  echo
  echo "  Does a batch run of joinerCheck -times"
  echo "  Choose one of three sets of databases:"
  echo "       all human assemblies [hg]"
  echo "       all mouse and rat assemblies [mm_rn]"
  echo "       all other assemblies [other]"
  echo
  echo "  usage: hg | mm_rn | other"
  echo
  exit 1
else
  set dbs = $argv[1]
endif

# make sure they made a good database choice
if ($dbs != 'hg' && $dbs != 'mm_rn' && $dbs != 'other') then
  echo
  echo "  I didn't understand your database choice."
  echo "  Choose between human [hg], mouse and rat [mm_rn], or all others [other]"
  echo
  exit 1
endif

set d = `date -u +%F`

# make a file for the output
set log = "batchTimes.$dbs.$d.log"

echo "Batch joinerCheck run (-times) for $dbs databases" >> $log
echo "Full reports in run dir `pwd`\n" >> $log
echo "Start All `date`\n" >> $log

# get the list of active databases on the RR
set active=`getRRdatabases.csh hgw1 | grep -v "last dump"`

# grep out of the $active list, the ones that they want to check during this run
if ($dbs == 'hg') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep hg`
endif

if ($dbs == 'mm_rn') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep 'mm\|rn'`
endif

if ($dbs == 'other') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep -v 'hg\|mm\|rn'`
endif

# now do it!
foreach db ($thisRun)
 echo "\nStart $db `date`" >> $log
    joinerCheck ~/kent/src/hg/makeDb/schema/all.joiner -database=$db -times >& $db.$d
 echo "End $db `date`" >> $log
end

echo "\n\nEnd All `date`" >> $log

mail -s "$log results" $who@soe.ucsc.edu < $log

