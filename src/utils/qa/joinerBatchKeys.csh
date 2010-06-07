#!/bin/tcsh 

##############################
#
# does joinerCheck -keys for a set a databases at a time 
#
##############################

set dbs = ''
set active = ''
set thisRun = ''

# usage statement
if ($#argv < 1 || $#argv > 2) then
  echo
  echo "  Does a batch run of joinerCheck -keys"
  echo "  Choose one of three sets of databases:"
  echo "       all human assemblies [hg]"
  echo "       all mouse and rat assemblies [mm_rn]"
  echo "       all other assemblies [other]"
  echo "       'extra' databases (e.g. go and proteins) [extra]"
  echo
  echo "  usage: hg | mm_rn | other | extra"
  echo
  exit 1
else
  set dbs = $argv[1]
endif

# make sure they made a good database choice
if ($dbs != 'hg' && $dbs != 'mm_rn' && $dbs != 'other' && $dbs != 'extra') then
  echo
  echo "  I didn't understand your database choice."
  echo "  Choose between human [hg], mouse and rat [mm_rn],"
  echo "  all others [other], or extra databases (e.g. proteins) [extra]"
  echo
  exit 1
endif

set d = `date -u +%F`

# make a file for the output
set log = "batchKeys.$dbs.$d.log"

echo "Batch joinerCheck run (-keys) for $dbs databases" >> $log
echo "Full reports in run dir `pwd`\n" >> $log
echo "Start `date`\n" >> $log

# get the list of active databases on the RR
set active=`getRRdatabases.csh hgw1 | grep -v "last dump"`

# grep out of the $active list, the ones that they want to check during this run
if ($dbs == 'hg') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep '^hg'`
endif

if ($dbs == 'mm_rn') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep '^mm\|^rn'`
endif

if ($dbs == 'other') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep -v '^hg\|^mm\|^rn\|^sp\|^go\|^prot\|^visi\|^uni\|^lost'`
endif

if ($dbs == 'extra') then
  set thisRun=`echo  $active | sed -e "s/ /\n/g" | grep '^sp\|^go\|^prot\|^visi\|^uni'`
endif

####################################################
# if you had to kill this script, and you now need to 
# pick-n-choose your databases, then add them here:
#set thisRun='droAna1 droAna2 droEre1 droGri1 droMoj1 droMoj2 droPer1 droSec1 droSim1 droVir1 droVir2 droYak1 droYak2 fr1 galGal2 galGal3 monDom1 monDom4 panTro1 panTro2 rheMac1 rheMac2 sacCer1 sc1 strPur1 tetNig1 xenTro1 xenTro2'
####################################################

# now do it
foreach db ($thisRun)
 echo "\nStart $db `date`" >> $log
    joinerCheck ~/kent/src/hg/makeDb/schema/all.joiner -database=$db -keys  >& $db.$d
 echo "End $db `date`" >> $log
end

echo "\n\nEnd All `date`" >> $log

mail -s "$log results" $USER@soe.ucsc.edu < $log

