#!/bin/tcsh

#######################
#
#  9-18-2008
#
#  get the assemblies to/from which a particular assembly has liftOver files.
#
#  Ann Zweig
#
#######################

set machine="hgw1"
set db=""
set central="hgcentral"
set host="-h genome-centdb"

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ($#argv < 1 || $#argv > 2 ) then
  echo
  echo "  get the assemblies to/from which a particular db has liftOver files."
  echo
  echo "    usage: db [hgwdev|hgwbeta]"
  echo "         defaults to RR machines"
  echo
  exit
endif


# assign command line arguments
set db=$argv[1]

if ($#argv == 2 ) then
  set machine=$argv[2]
endif

if ($machine == 'hgwbeta') then
 set central='hgcentralbeta'
 set host='-h hgwbeta'
endif
if ($machine == 'hgwdev') then
 set central='hgcentraltest'
 set host='-h hgwdev'
endif

echo "\nliftOver chains TO $db on $machine --\n" 
hgsql $host -Ne "SELECT fromDb FROM liftOverChain WHERE toDb = '$db' \
 ORDER BY fromDb" $central

echo "\nliftOver chains FROM $db on $machine --\n" 
hgsql $host -Ne "SELECT toDb FROM liftOverChain WHERE fromDb = '$db' \
 ORDER BY toDb" $central
