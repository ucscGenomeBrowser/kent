#!/bin/tcsh

###############################################
# 
#  08-30-07
#  Robert Kuhn & Ann Zweig
#
#  Runs through the usual checks for IGTC monthly update
# 
###############################################

set update=""
set year=""
set month=""
set day=""
set mice="mm8 mm7 mm6"
set extFileId_mm8=""
set extFileId_mm7=""
set extFileId_mm6=""
set pwd=""

if ($#argv != 1 ) then
  echo
  echo "  runs test suite for IGTC update on mice."
  echo "  hard-coded to run on mm6, mm7, and mm8."
  echo
  echo "    usage: date"
  echo "    ('date' of associated fasta files -- format = yyyy-mm-dd)"
  echo
  exit
else
  set update=$argv[1]
endif

# parse the date
set year=`echo $update | awk -F- '{print $1}'`
set month=`echo $update | awk -F- '{print $2}'`
set day=`echo $update | awk -F- '{print $3}'`

# make sure they have entered something close to correct (kludge warning!)
if ( $year < 2007 || $year > 2020 ) then
  echo "\nERROR: the 'date' format is incorrect"
  $0
  exit 1
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\nERROR: you must run this script on hgwdev!\n"
  exit 1
endif

set pwd=`pwd`

# Explain what we're QAing
echo "Running on these mouse assmeblies:"
echo "  $mice"
echo "\nAnd on these files:"
foreach mouse ( $mice )
  echo "  /gbdb/$mouse/igtc/genetrap.$update.fasta"
end


# make sure the files exist
foreach mouse ( $mice )
  if (! -e ~/gbdb/$mouse/igtc/genetrap.$update.fasta) then
    echo "\nERROR: can't find the file: genetrap.$update.fasta\n"
    exit 1
  endif
end 


echo "\n\n----------------------"
echo "run Joiner Check"
joinerCheck -keys -identifier=igtcName ~/kent/src/hg/makeDb/schema/all.joiner


echo "\n\n----------------------"
echo "compare new (dev) and old (beta) tables"
echo
foreach mouse ( $mice )
  compareWholeColumn.csh $mouse igtc qName
end


echo "\n\n----------------------"
echo "check a few of the new additions to the igtc table"
echo "(be sure to click all the way out to IGTC website)"
foreach mouse ( $mice )
  echo " \n\ncheck these in $mouse browser on hgwdev:"
  head -3 $mouse.igtc.qName.devOnly
  tail -3 $mouse.igtc.qName.devOnly
end


echo "\n\n----------------------"
echo "here's a list of the types of items being deleted from the igtc table"
foreach mouse ( $mice )
  echo "from $mouse"
  cat $mouse.igtc.qName.betaOnly | awk -F"_" '{print $NF}' | sort | uniq -c | sort
end


echo "\n\n----------------------"
echo "check a few of the deleted items from the igtc table"
echo "(make sure they have also been dropped from IGTC website)"
foreach mouse ( $mice )
  echo " \n\ncheck these in $mouse browser on hgwbeta:"
  head -3 $mouse.igtc.qName.betaOnly
  tail -3 $mouse.igtc.qName.betaOnly
end


echo "\n\n----------------------"
echo "check out a few new elements that hit on all 3 assemblies"
echo "(get the DNA and BLAT it to the others)"
cat mm8.igtc.qName.dev mm7.igtc.qName.dev mm6.igtc.qName.dev | sort | uniq -c > allDev
cat allDev | sed -e "s/^ *//" | grep ^3 | awk '{print $2}' | head -3
cat allDev | sed -e "s/^ *//" | grep ^3 | awk '{print $2}' | tail -3


echo "\n\n----------------------"
echo "check out a few elements that were dropped from all 3 assemblies"
cat mm8.igtc.qName.beta mm7.igtc.qName.beta mm6.igtc.qName.beta | sort | uniq -c > allBeta
cat allBeta | sed -e "s/^ *//" | grep ^3 | awk '{print $2}' | head -3
cat allBeta | sed -e "s/^ *//" | grep ^3 | awk '{print $2}' | tail -3


echo "\n\n----------------------"
echo "----------------------"
echo "Now for the seq & extFile table magic..."
# print what's currently in the extFile table on dev and beta for all mice
echo "Contents of extFile table on dev and beta:"
echo "(what you should expect to see is this month's file on dev and"
echo "last month's file on beta)"
foreach mouse ( $mice )
  echo
  echo "$mouse.extFile on hgwdev:"
  hgsql -e 'SELECT * FROM extFile WHERE name LIKE "genetrap%"' $mouse
  echo "\n$mouse.extFile on hgwbeta:"
  hgsql -h hgwbeta -e 'SELECT * FROM extFile WHERE name LIKE "genetrap%"' $mouse
  echo
end


# get the extFile.id value for this month's entry
echo "\n\n----------------------"
echo "You will need to know the id field from the extFile table entry"
echo "for this month's file (genetrap.$update.fasta):"

### ??? can't get the loop to work with $mice variable ??? ###
#foreach mouse ( $mice )
#  set extFileId_${mouse}=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE 'genetrap.$update.fasta'" $mouse`
#  echo "$mouse.extFile.id = $extFileId_$mouse"
#end

### So, I'll just do them one-by-one for now...
set extFileId_mm8=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE 'genetrap.$update%'" mm8`
echo "mm8.extFile.id = $extFileId_mm8"

set extFileId_mm7=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE 'genetrap.$update%'" mm7`
echo "mm7.extFile.id = $extFileId_mm7"

set extFileId_mm6=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE 'genetrap.$update%'" mm6`
echo "mm6.extFile.id = $extFileId_mm6"



# compare counts with file counts
echo "\n\n----------------------"
echo "Sanity check: compare counts in seq table (on dev) with what's in the file"
echo "(if the values aren't equal per assembly, this needs investigation)\n"

### ??? can't get the loop to work with $mice variable ??? ###
# So, just do it one-by-one for now...
echo "compare count from mm8.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm8'"' mm8
echo "to count from mm8 genetrap.$update.fasta file:"
cd /gbdb/mm8/igtc/
grep '>' genetrap.$update.fasta | wc -l
echo

echo "compare count from mm7.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm7'"' mm7
echo "to count from mm7 genetrap.$update.fasta file:"
cd /gbdb/mm7/igtc/
grep '>' genetrap.$update.fasta | wc -l
echo

echo "compare count from mm6.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm6'"' mm6
echo "to count from mm6 genetrap.$update.fasta file:"
cd /gbdb/mm6/igtc/
grep '>' genetrap.$update.fasta | wc -l
echo

cd $pwd



# get the new rows from tables on dev
echo "\n\n----------------------"
echo "Creating a file with the new rows from the extFile and seq tables on dev"
echo "(these will need to be moved to beta)\n"

echo "The files are here:"
foreach mouse ( $mice)
  echo "$pwd/$mouse.seq.$update"
  echo "$pwd/$mouse.extFile.$update"
end

### ??? can't get the loop to work with $mice variable ??? ###
# So, just do it one-by-one for now...
hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm8'" mm8 > mm8.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" mm8 > mm8.extFile.$update

hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm7'" mm7 > mm7.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" mm7 > mm7.extFile.$update

hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm6'" mm6 > mm6.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" mm6 > mm6.extFile.$update



# delete the rows from tables on beta
echo "\n\n----------------------"
echo "Here are the commands needed to delete the rows from the extFile and seq tables on BETA\n"

# Need to subtract one from the $month variable (so that it's last month's date)
# e.g. set lastMonthUpdate=$update minus one month (!)

# Need to find the seq.id for last month's data...

#foreach mouse ( $mice )
#  echo "DELETE FROM $mouse.extFile WHERE name LIKE 'genetrap.$lastMonthUpdate.fasta';"
#  echo "DELETE FROM $mouse.seq WHERE extFile = '$lastMonthExtFileId_${mouse}';"
#end


# delete the rows from tables on dev
echo "\n\n----------------------"
echo "Here are the commands needed to delete the rows from the extFile and seq tables on DEV"
echo "(these are last month's data)"

#foreach mouse ( $mice )
#  echo "DELETE FROM $mouse.extFile WHERE name LIKE 'genetrap.$lastMonthUpdate.fasta';"
#  echo "DELETE FROM $mouse.seq WHERE extFile = '$lastMonthExtFileId_${mouse}';"
#end



# load rows into tables on beta
echo "\n\n----------------------"
echo "Here are the commands needed to load the rows into extFile and seq tables on BETA\n"

#foreach mouse ( $mice )
#  echo "hgsql -h hgwbeta -e 'LOAD DATA LOCAL INFILE "'$mouse.extFile.$update'" INTO TABLE extFile' $mouse"
#  echo "hgsql -h hgwbeta -e 'LOAD DATA LOCAL INFILE "'$mouse.seq.$update'" INTO TABLE seq' $mouse"
#end





echo "\n\n----------------------"
echo "----------------------"
echo "other things to check/do:"
echo " - make sure the IGTC site links back to ours correctly"
echo " - push tables to hgwbeta:"
foreach mouse ( $mice )
  echo "    sudo mypush $mouse igtc hgwbeta"
end
echo " - ask for push of files from hgwdev to hgnfs1:"
foreach mouse ( $mice )
  echo "   /gbdb/$mouse/igtc/genetrap.$update.fasta"
end

echo "\nthe end."

# clean up
cd $pwd
rm allDev
rm allBeta

exit










##############################################
### FROM BOB's original script -- NOT WORKING
# ---------------------------
echo "get extFile ids"

set j=8

while ( $j > 5)
  set id[$j]="yes"
  echo mm$j $i{id[$j})
end

  # vv---------broken
foreach j ( 6 7 8 )
  echo mm$j
  set id[$j]=`hgsql -Ne 'SELECT id, name FROM extFile \
    WHERE name LIKE "genetrap%'$year'%'$month'%"' $mouse`
  hgsql -Ne 'SELECT id FROM extFile \
    WHERE name LIKE "genetrap%'$year'%'$month'%"' $mouse
  # echo ${${mouse}Id}  # <<---------broken
  # echo $id[$mouse]
echo here
  echo $id[$mouse]
end


