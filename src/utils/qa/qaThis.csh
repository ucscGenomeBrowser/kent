#!/bin/tcsh
source `which qaConfig.csh`

# This script runs qa scripts on a table

# enter the name of the database and the name of the table

set db=''
set tableList=''
set table =''
set devTime=""
set betaTime=""
set rrTime=""
set countPerChr=""

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
  echo "    In that case, it creates tableList.db.qa"
  echo 
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
     $0 $db $table >>& $db.$tableList.qa
   end
 else  
   foreach table ( $tables )
     $0 $db $table c >>& $db.$tableList.qa
   end
 endif  
else
  set tables=$tableList

#--------------------------------------------------
#Display Table name  
echo
echo "~!~!~!~!~!~!~!~!~!~!~!~!~!~!~!~ "
echo   "$tables"
echo
echo "~!~!~!~!~!~!~!~!~!~!~!~!~!~!~!~ "
echo

# ------------------------------------------------
#Verify make doc:
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Verify the makedoc at:"
echo "~/kent/src/hg/makeDb/doc/$db.txt"
echo

#------------------------------------------------
#check getSplit.csh 
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Is this table split (using getSplit.csh)"

getSplit.csh $db $tables hgwdev 

# ------------------------------------------------
# check updateTimes for each table:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo
set devTime=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$tables'"' $db \
   | awk '{print $14, $15}'`
set betaTime=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$tables'"' $db \
   | awk '{print $14, $15}'`
set rrTime=`getRRtableStatus.csh $db $tables Update_time`
if ( $status ) then
    set rrTime=""
endif    
echo "Check updateTimes for each table:"
echo "Dev:  $devTime "
echo "Beta: $betaTime"
echo "RR:   $rrTime" 


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
featureBits -countGaps $db $tables gap 

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

#----------------------------------------------
#Show the first 3 rows
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Here are the first three rows of the table:"
hgsql -e "select * from ${tables} limit 3" $db
echo

#------------------------------------------------
#Count per Chrom 
  if ( $countPerChr == "c" ) then
    echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
    echo "Check countPerChrom.csh"  
    countPerChrom.csh $db $tables
    echo
  endif
endif

exit 0


