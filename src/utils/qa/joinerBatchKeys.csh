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
#set thisRun='ailMel1 anoCar1 anoCar2 anoGam1 apiMel1 apiMel2 apiMel3 aplCal1 bosTau1 bosTau2 bosTau3 bosTau4 bosTau5 bosTau6 bosTau7 bosTauMd3 braFlo1 braFlo2 caeJap1 caePb1 caePb2 caeRem2 caeRem3 calJac1 calJac3 canFam1 canFam2 canFam3 cavPor2 cavPor3 cb1 cb3 ce10 ce2 ce4 ce6 cerSim1 choHof1 chrPic1 ci1 ci2 danRer1 danRer2 danRer3 danRer4 danRer5 danRer6 danRer7 dasNov1 dasNov2 dasNov3 dipOrd1 dm1 dm2 dm3 dp2 dp3 dp4 droAna1 droAna2 droAna3 droEre1 droEre2 droGri1 droGri2 droMoj1 droMoj2 droMoj3 droPer1 droSec1 droSim1 droVir1 droVir2 droVir3 droWil1 droYak1 droYak2 echTel1 equCab1 equCab2 eriEur1 felCat3 felCat4 felCat5 fr1 fr2 fr3 gadMor1 galGal2 galGal3 galGal4 gasAcu1 geoFor1 gorGor1 gorGor3 hetGla1 hetGla2 latCha1 loxAfr1 loxAfr2 loxAfr3 macEug1 macEug2 melGal1 melUnd1 micMur1 monDom1 monDom2 monDom4 monDom5 myoLuc1 myoLuc2 nomLeu1 nomLeu2 ochPri2 oreNil1 oreNil2 ornAna1 oryCun1 oryCun2 oryLat1 oryLat2 otoGar1 otoGar3 oviAri1 panTro1 panTro2 panTro3 panTro4 papAnu2 papHam1 petMar1 petMar2 ponAbe2 priPac1 proCap1 pteVam1 rheMac1 rheMac2 rheMac3 sacCer1 sacCer2 sacCer3 saiBol1 sarHar1 sorAra1 speTri1 speTri2 strPur1 strPur2 susScr2 susScr3 taeGut1 tarSyr1 tetNig1 tetNig2 triCas1 triCas2 triMan1 tupBel1 turTru1 turTru2 vicPac1 xenTro1 xenTro2 xenTro3'
####################################################

# now do it
foreach db ($thisRun)
 echo "\nStart $db `date`" >> $log
    joinerCheck ~/kent/src/hg/makeDb/schema/all.joiner -database=$db -keys  >& $db.$d
 echo "End $db `date`" >> $log
end

echo "\n\nEnd All `date`" >> $log

mail -s "$log results" $USER@ucsc.edu < $log

