#!/bin/tcsh
source `which qaConfig.csh`

################################
#
# 04-23-2009
# Ann Zweig
#
# This script is to be used to check files against their corresponding table. 
# It accepts a table name that has a file associated with it. It makes sure 
# that the file corresponds correctly with its table by comparing total count
# as well as size (sum(chromEnd-chromStart)).
#
################################

set db=""
set table=""
set file=""
set verb="F"

# usage statement
if ( $#argv < 3 || $#argv > 4 ) then
 echo
 echo " Ensures that a table correlates with its associated file."
 echo " Only prints results if there is a diff between table and file."
 echo " Works for these file types: narrowPeak, broadPeak, gappedPeak,"
 echo "                             bedGraph, NRE, BiP, gcf"
 echo " (for wiggle files use: checkWigFiles.csh)"
 echo
 echo "  usage:  database tableName fileName [verbose]"
 echo "   fileName includes path of download file "
 echo "   e.g. /goldenPath/<db>/fileName.gz"
 echo
 echo "   use verbose for more details"
 echo
 exit
endif

set db=$argv[1]
set table=$argv[2]
set file=$argv[3]

if ( $#argv == 4 ) then
 set verb="T"
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\nERROR: this script must be run from hgwdev."
 exit 1
endif

if ( ! -e $file ) then
 echo " \nERROR: sorry I can't find the original file here: ~$file\n"
 exit 1
endif

# get count and size (sum(chromEnd-chromStart))from $file
set numF=`zcat $file | egrep -v "track" | gawk '{++n} END {print n}'`
set sizeF=`zcat $file | gawk '{size+=$3-$2} END {print size}'`

# get count and size (sum(chromEnd-chromStart)) from $table
set numT=`hgsql -Ne "SELECT count(*) FROM $table" $db`
set sizeT=`hgsql -Ne "SELECT sum(chromEnd-chromStart) FROM $table" $db`

# compare output counts and sizes
if ( $numF != $numT ) then
 echo "\nDIFFERENCES! file count = $numF, table count = $numT\n"
endif
if ( $sizeF != $sizeT ) then
 echo "\nDIFFERENCES! file size = $sizeF, table size = $sizeT\n"
endif

if ( $verb == "T" ) then
 echo "\ncount for file: $numF"
 echo "count for table: $numT"
 echo
 echo "(sum(chromEnd-chromStart)) for file: $sizeF"
 echo "(sum(chromEnd-chromStart)) for table: $sizeT\n"
endif

exit 0 
