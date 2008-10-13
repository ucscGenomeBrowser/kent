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

set dirPath="/usr/local/apache/htdocs"
set machine="genome-test.cse.ucsc.edu"
set directory="qa/test-results/staticLinks"
set dateString=`date +%Y-%m-%d`

if ( ! -d $dirPath/$directory/$dateString ) then
  mkdir $dirPath/$directory/$dateString
endif

nice checkAllStaticLinks.csh all $dateString >& $dirPath/$directory/$dateString/links.$dateString
if ( $status ) then
  echo "\nerror.  possibly exclude file does not exist.  quitting.\n"
  exit 1
endif

echo "link-checking complete"
date
echo "results at:"
echo "http://$machine/$directory/$dateString/linkCheck.all.$dateString"


