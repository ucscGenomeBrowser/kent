#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  02-08-07
#  Robert Kuhn
#
#  gets MySql version directly from database
#
################################

set machine=""
set part=""

if ( $#argv < 1 ||  $#argv > 2 ) then
  echo
  echo "  gets MySql version directly from database."
  echo
  echo "    usage:  machine [1 2 3]"
  echo "      1,2 & 3 return version and sub-version values only"
  echo
  exit
else
  set machine=$argv[1]
endif

if ( 2 == $#argv ) then
  set part=$argv[2]
endif

checkMachineName.csh $machine
if ( $status ) then
  exit 1
endif

set url1="http://"
# set url2=".soe.ucsc.edu/cgi-bin/hgTables?hgta_doMysqlVersion=1"
if ( $machine == genome-euro || $machine == genome-asia ) then
  set url2=".ucsc.edu/cgi-bin/hgTables?hgta_doMetaData=1&hgta_metaVersion=1"
else
  set url2=".soe.ucsc.edu/cgi-bin/hgTables?hgta_doMetaData=1&hgta_metaVersion=1"
endif
set url="$url1$machine$url2"
set version=`wget -q -O /dev/stdout "$url"`
set version=`echo $version | awk -F" " '{print $2}'`
if (! $part ) then
  echo $version | grep .
else
  if ( 1 == $part ) then
    echo $version | grep . | awk -F. '{print $1}'
  else
    if ( 2 == $part ) then
      echo $version | grep . | awk -F. '{print $2}'
    else
      if ( 3 == $part ) then
        echo $version | grep . | awk -F. '{print $3}' \
         | awk -F- '{print $1}'
      else
        echo
        echo "  improper parameter:  $part"
        echo
        echo "$0"
        $0
      endif
    endif
  endif
endif
# rm -f $machine.version

