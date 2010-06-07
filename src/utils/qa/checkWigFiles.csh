#!/bin/tcsh
source `which qaConfig.csh`

################################
#
# 04-23-2009
# Ann Zweig
#
# This script is used to check wiggle files. It accepts a table name
# that has a wiggle file associated with it. It makes sure that the wiggle 
# file corresponds correctly with its table.
#
################################

set db=""
set table=""
set file=""
set remove="F"

# usage statement
if ( $#argv < 3 || $#argv > 4 ) then
 echo
 echo " Ensures that a table correlates with its wig file."
 echo " Only prints results if there is a problem."
 echo
 echo "  usage:  database tableName fileName [rm]"
 echo "   fileName includes path of wig download file "
 echo "    (.txt and/or .gz files are OK)"
 echo
 echo "    e.g. /usr/local/apache/htdocs-hgdownload/goldenPath/<db>/wig/fileName.wig"
 echo
 echo "    add rm if you would like the output files to be removed."
 echo
 exit
else
 set db=$argv[1]
 set table=$argv[2]
 set file=$argv[3]
 if ( "rm" == $argv[4] ) then
  set remove="T"
 else
  echo "\nERROR: fourth argument must be 'rm'\n"
  exit 1
 endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\nERROR: this script must be run from hgwdev.\n"
 exit 1
endif

if ( ! -e $file ) then
 echo "\nERROR: sorry I can't find the original wig file here:"
 echo " $file\n"
 exit 1
endif

# convert Wiggle ascii data file to binary format
wigEncode $file test.$table.wig test.$table.wib

# fetch wiggle data from table and perform stats
hgWiggle -db=$db -doStats $table | grep -v '^#' > stats.$table.table

# fetch wiggle data from file and perform stats
hgWiggle -doStats test.$table.wig | grep -v '^#' > stats.$table.file

# compare output files from two hgWiggle steps above
diff -q stats.$table.file stats.$table.table

if ( "F" != $remove ) then
 rm -f test.$table.wig test.$table.wib stats.$table.*
endif

exit 0 
