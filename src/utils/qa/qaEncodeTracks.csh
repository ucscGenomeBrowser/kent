#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  03-05-2009
#  Ann Zweig
#
#  Runs through the usual checks for ENCODE
#  tables.
# 
###############################################

set db=''
set tableList=''

if ($#argv != 2 ) then
  echo
  echo "  runs test suite for ENCODE tracks"
  echo "  (it's best to direct output and errors to a file: '>&')"
  echo
  echo "    usage: db tableList"
  echo
  exit 1
else
  set db=$argv[1]
  set tableList=$argv[2]
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\nERROR: you must run this script on hgwdev!\n"
  exit 1
endif

# check to see if it is a single tableName or a tableList
file $tableList | egrep "ASCII text" > /dev/null
if (! $status) then
 set tables=`cat $tableList`
else
 set tables=$tableList
endif


# featureBits for all tables
#echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#echo "*** featureBits for all tables ***"
#foreach table ( $tables )
# echo ""
# echo "featureBits -countGaps $db $table"
# nice featureBits -countGaps $db $table
# echo "featureBits -countGaps $db $table gap"
# nice featureBits -countGaps $db $table gap
#end

# check for table descriptions for all tables
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** check for tableDescription entry for all tables ***"
echo "Is there an entry in the tableDescriptions table for each"
echo "table? (1 == yes, 0 == no)"
foreach table ( $tables )
 echo ""
 echo "Table: $table"
 hgsql -Ne "select count(*) from tableDescriptions where tableName = '$table'" $db
end

# no underscores in table names
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** make sure there are no underscores in table names ***"
echo "If there's output here, you have one or more tables with "_":"
echo $tables | grep "_"
echo ""

# check that positional tables are sorted for all tables
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** check that positional tables are sorted ***"
echo "(No output means table is sorted)"
foreach table ( $tables )
 echo ""
 echo "positionalTblCheck $db $table"
 positionalTblCheck $db $table
end

# check table index for each table
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** Check table INDEX ***"
foreach table ( $tables )
 echo ""
 echo ""
 hgsql -e "SHOW INDEX FROM $table" $db
end

# checkTableCoords for each table (instead of checkOffend.csh)
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** checkTableCoords for each table ***"
foreach table ( $tables )
 checkTableCoords $db $table
end
echo "\n(Nothing will be printed if all are okay.)"

# check the length of the shortLabel for each track
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** MAX length of shortLabel should be 16 ***"
foreach table ( $tables )
 echo "Table: $table"
 cat ~/trackDb/human/hg18/trackDb.wgEncode.ra | grep -A10 "track $table" | grep -m 1 shortLabel \
  | sed -e 's/shortLabel //' | sed -e 's/^ *//' | sed -e 's/.$//' | wc -m
end

# check the length of the longLabel for each track
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** MAX length of longLabel should be 80 ***"
foreach table ( $tables )
 echo "Table: $table"
 cat ~/trackDb/human/hg18/trackDb.wgEncode.ra | grep -A10 "track $table" \
  | grep -m 1 longLabel \
  | sed -e 's/longLabel //' | sed -e 's/^ *//' | sed -e 's/.$//' | wc -m
end

# countPerChrom for all tables
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** countPerChrom for all tables ***"
foreach table ( $tables )
 echo ""
 echo "countPerChrom.csh $db $table"
 countPerChrom.csh $db $table
end

echo "\nthe end.\n"
exit 0
