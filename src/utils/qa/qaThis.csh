#!/bin/tcsh
source `which qaConfig.csh`

# This script runs qa scripts on a table

# enter the name of the database and the name of the table

set db=''
set tableList=''
set table =''
set first=""
set second=""
#set third=""
set fourth=""
set countPerChr=""

#if ($#argv == 1) then
#  if ( $argv[1] == "CLEAN" || $argv[1] == "clean" ) then 
#  rm *.qa
#  rm *.gapFile
#  exit 0
#  endif
#endif

if ($#argv < 2 || $#argv > 3) then
  echo
  echo " Runs basic QA scripts on your table(s)"
  echo
  echo "    usage:  database Tablename [c]"
  echo
  echo "    c runs count per chrom"                                         
  echo
  echo "    If redirecting output, be sure"
  echo "    to redirect stdout and stderr (>&) "
  echo
  echo "    Works if you provide a list of tables"
  echo "    In that case, it creates db.tableName.qa files"
  echo 
#  echo "    Added clean-up functionality to remove all .qa and .gapFile files"
#  echo "    Usage:   CLEAN or clean"
  echo

  exit
else
  set db=$argv[1]
  set tableList=$argv[2]
  
  if ( $#argv == 3 ) then
    set countPerChr=$argv[3]
  endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

file $tableList | egrep "ASCII text" > /dev/null
if (! $status) then
 set tables=`cat $tableList`
 if ( $countPerChr != "c" ) then
   foreach table ( $tables )
     $0 $db $table >& $db.$table.qa
   end
 else  
   foreach table ( $tables )
     $0 $db $table c >& $db.$table.qa
   end
 endif  
else
  set tables=$tableList

#------------------------------------------------
#check getSplit.csh 
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Is this table split (using getSplit.csh)"

getSplit.csh $db $tables hgwdev 

# ------------------------------------------------
# check updateTimes for each table:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo
set first=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$tables'"' $db \
   | awk '{print $14, $15}'`
set second=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$tables'"' $db \
   | awk '{print $14, $15}'`
set fourth=`getRRtableStatus.csh $db $tables Update_time`
if ( $status ) then
    set fourth=""
endif    
echo "Check updateTimes for each table:"
echo "Dev:  $first "
echo "Beta: $second"
echo "RR:   $fourth" 


# ------------------------------------------------
# check FeatureBits for table:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Check FeatureBits(second is vs. gap) : "
echo "featureBits -countGaps $db $tables"
featureBits -countGaps $db $tables

if ($status) then
  echo "failed."
  echo
endif

echo "featureBits -countGaps $db $tables gap"
featureBits -countGaps $db $tables gap -bed=$db.$tables.gapFile
echo
if ( -z $db.$tables.gapFile ) then
echo
rm -f $db.$tables.gapFile
else
echo "There are gaps overlapping $tables (gaps may be bridged or not):"
echo "Gap file is here: $db.gapFile" 
endif
		    
# ------------------------------------------------
# check Table sort for table:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Check table sort (only prints if errors):"
positionalTblCheck $db $tables

# ------------------------------------------------
# check if any values are off Chrom ends

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Check for illegal genomic coordinates (only prints if errors):"
checkTableCoords $db -table=$tables

#----------------------------------------------
#Check indices
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check indices:"
hgsql -e "show index from ${tables}" $db

#-----------------------------------------------
#joiner check
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Joiner check:"
runJoiner.csh $db $tables noTimes

# ------------------------------------------------
# check level for html and trackDb entry:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check level for html and trackDb entry:"
findLevel $db $tables

# ------------------------------------------------
#Verify make doc:
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Verify the makedoc at:"
echo "~/kent/src/hg/makeDb/doc/$db.txt"

#----------------------------------------------
#Show the first 2 rows
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Here are the first two rows of the table:"
hgsql -e "select * from ${tables} limit 2" $db

#------------------------------------------------
#Count per Chrom 
  if ( $countPerChr == "c" ) then
    echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
    echo "Check countPerChrom.csh"  
    countPerChrom.csh $db $tables
  endif
endif

exit 0


