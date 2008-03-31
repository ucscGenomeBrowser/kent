#!/bin/tcsh

################################
#  
#  03-31-2008
#  Ann Zweig
#
#  Find the organism name given the assembly name
#
################################


if ( $#argv != 1 ) then
  echo
  echo " Finds the organism name given the assembly name" 
  echo "  usage: assemblyName"
  echo "         will accept name with or without digit"
  echo "         (e.g. 'ornAna2' or 'ornAna')"
  echo
  exit 1
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n ERROR: you must run this script on dev!\n"
 exit 1
endif

hgsql -Ne "SELECT name, organism FROM dbDb WHERE NAME LIKE '$db%'" hgcentraltest

exit 0
