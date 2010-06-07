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
set maxShortLabel='17'
set maxLongLabel='80'

if ($#argv != 2 ) then
  echo
  echo " Runs test suite for ENCODE tracks"
  echo " (it's best to direct output and errors to a file: '>&')"
  echo " In general, this script only prints output if there are problems" 
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

# Takes too long to run (commented out for now):
# featureBits for all tables
#echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
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
echo "*** check for tableDescription entry for all tables ***"
echo "(Only prints tablename if there is NO entry in tableDescription table)"
foreach table ( $tables )
 set num=`hgsql -Ne "select count(*) from tableDescriptions where tableName = '$table'" $db`
 if ( 0 == $num ) then
  echo "\nERROR: no description for $table"
 endif
end

# no underscores in table names
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** make sure there are no underscores in table names ***"
echo "(If there's output here, you have one or more tables with "_")"
echo $tables | grep "_"

# check that positional tables are sorted for all tables
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** check that positional tables are sorted ***"
echo "(Only prints if the table is NOT sorted)"
foreach table ( $tables )
 positionalTblCheck $db $table
end

# check table index for each table
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** Check table INDEX ***"
echo "(Only prints the INDEX if there are fewer than two indices."
echo "Presumably, these two will include bin and something else."
echo "Note that bbi, bigBed and bigWig tables will have no index.)"
foreach table ( $tables )
 set num=`hgsql -N -e "SHOW INDEX FROM $table" $db | wc -l`
 if ( $num < 2 ) then
  echo "ERROR: $table has fewer than 2 indices"
 endif 
end

# checkTableCoords for each table (instead of checkOffend.csh)
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** checkTableCoords for each table ***"
echo "(Only prints if there are illegal coordinates in table)"
foreach table ( $tables )
 checkTableCoords $db $table
end

# check the length of the shortLabel for each track
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** MAX length of shortLabel should be 17 ***"
echo "(Only prints if shortLabel is greater than 17 characters, or if"
echo "it can't find a shortLabel at all)"
foreach table ( $tables )
 set num=`tdbQuery "select shortLabel from $db where track = '$table'" \
  | sed -e 's/shortLabel //' | sed -e '/^[ ]*$/d' | awk '{print length}'`
 if ( '' == $num ) then
  echo "ERROR: can't find entry in trackDb for $table"
 else
  if ( $maxShortLabel < $num || 0 == $num ) then
   if ( 0 == $num ) then
    echo "ERROR: can't find a shortLabel for $table"
   else
    echo "ERROR: $table shortLabel is $num characters"
   endif
  endif
 endif
end

# check the length of the longLabel for each track
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** MAX length of longLabel should be 80 ***"
echo "(Only prints if longLabel is greater than 80 characters, or if"
echo "it can't find a longLabel at all)"
foreach table ( $tables )
 set num=`tdbQuery "select longLabel from $db where track = '$table'" \
  | sed -e 's/longLabel //' | sed -e '/^[ ]*$/d' | awk '{print length}'`
 if ( '' == $num ) then
  echo "ERROR: can't find entry in trackDb for $table"
 else
  if ( $maxLongLabel < $num || 0 == $num ) then
   if ( 0 == $num ) then
    echo "ERROR: can't find a longLabel for $table"
   else
    echo "ERROR: $table longLabel is $num characters"
   endif
  endif
 endif
end

# countPerChrom for all tables
echo "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "*** countPerChrom for all tables ***"
echo "(prints all counts here -- a useful way to look at this output is to"
echo "grep for all 'chr4', for example)"
foreach table ( $tables )
 echo ""
 echo "countPerChrom.csh $db $table"
 countPerChrom.csh $db $table
end

echo "\nthe end.\n"
exit 0
