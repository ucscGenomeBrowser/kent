#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  02-06-07
#  Robert Kuhn
#
#  gets TABLE STATUS from a machine in real time using wget
#
################################

set db=""
set mach="hgwbeta"
set url1=""
set url2=""
set url2a=""
set url3=""
set url=""

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  gets database TABLE STATUS from a machine in real time using wget."
  echo "  note:  currently only works for dbs naming an assembly."
  echo
  echo "    usage:  database [machine]"
  echo "              defaults to beta"
  echo
  exit
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif


if ( $#argv == 2 ) then
  set mach=$argv[2]
endif

if ( $mach == euronode ) then
  set mach="genome-euro"
endif

checkMachineName.csh $mach
if ( $status ) then
  exit 1
endif

# get the STATUS
set url1="http://"
set url2a=".soe"
set url2=".ucsc.edu/cgi-bin/hgTables?db=$db&hgta_doMetaData=1"
set url3="&hgta_metaStatus=1"
if ( $mach == genome-euro || $mach == genome-asia ) then
  set url="$url1$mach$url2$url3"
else
  set url="$url1$mach$url2a$url2$url3"
endif

wget -q -O $mach.tempfile "$url"
if ( $db != `head -1 $mach.tempfile | awk '{print $NF}'` ) then
  # does not allow uniProt or visiGene, etc to work.  
  # hgTables.doMetaData sets the db variable to hg18 if not an assembly
  echo
  echo "  $db is not a valid database on $mach"
  echo
  rm -f $mach.tempfile
  exit 1
endif
cat $mach.tempfile
rm -f $mach.tempfile
exit

