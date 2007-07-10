#!/bin/tcsh

################################
#  
#  02-24-07
#  Robert Kuhn & Archana Thakkapallayil
#
#  runs linkChecker automatically
#
################################

set dateString=""

if ( $#argv != 1 ) then
  echo
  echo "  runs linkChecker automatically"
  echo
  echo "    usage:  go"
  echo
  exit
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set directory="/usr/local/apache/htdocs/qa/test-results/staticLinks"
cd $directory
set dateString=`date +%Y-%m-%d`

if ( ! -d $dateString ) then
  mkdir $dateString
endif
cd $dateString
nice checkAllStaticLinks.csh all $dateString >& links.$dateString &

echo "link-checking complete"
date
echo "results at:"
echo "http://$directory/$dateString/linkCheck.all.$dateString"


