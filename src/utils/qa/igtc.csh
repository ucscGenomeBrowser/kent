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
set mice="mm9 mm8 mm7"
set extFileId_mm9=""
set extFileId_mm8=""
set extFileId_mm7=""
set pwd=""
set urlbeta="http://hgwbeta.cse.ucsc.edu/cgi-bin/hgTracks?igtc=pack"
set urldev="http://hgwdev.cse.ucsc.edu/cgi-bin/hgTracks?igtc=pack"

if ($#argv != 1 ) then
  echo
  echo "  runs test suite for IGTC update on mice."
  echo "  hard-coded to run on mm7, mm8, and mm9."
  echo
  echo "    usage: date"
  echo '    ("date" of associated fasta files -- format = yyyy-mm-dd)'
  echo
  exit
else
  set update=$argv[1]
endif

# parse the date
set year=`echo $update | awk -F- '{print $1}'`
set month=`echo $update | awk -F- '{print $2}'`
set day=`echo $update | awk -F- '{print $3}'`

if ( 0 == 0 ) then  ##  not-run block

# make sure they have entered something close to correct (kludge warning!)
if ( $year != `date +%Y` || $month > 12 || $day > 31 ) then
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
echo "\nRunning on these mouse assemblies:"
echo "  $mice"
echo "\nAnd on these files:"
foreach mouse ( $mice )
  echo "  /gbdb/$mouse/igtc/genetrap.$update.fasta"
end

# make sure the files exist
foreach mouse ( $mice )
  if (! -e /gbdb/$mouse/igtc/genetrap.$update.fasta) then
    echo "\nERROR: can't find the file: \
      /gbdb/$mouse/igtc/genetrap.$update.fasta\n"
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
  echo " \n\ncheck these in $mouse browser on hgwdev (or use links below):"
  head -3 $mouse.igtc.qName.devOnly
  tail -3 $mouse.igtc.qName.devOnly
end

echo "\n\n----------------------"
echo "here's a list of the types of items being deleted from the igtc table"
foreach mouse ( $mice )
  echo "from $mouse"
  cat $mouse.igtc.qName.betaOnly | awk -F"_" '{print $NF}' | sort | uniq -c \
    | sort
end


echo "\n\n----------------------"
echo "check a few of the deleted items from the igtc table"
echo "(make sure they have also been dropped from IGTC website)"
foreach mouse ( $mice )
  echo " \n\ncheck these in $mouse browser on hgwbeta:"
  head -3 $mouse.igtc.qName.betaOnly 
  set sample=`head -3 $mouse.igtc.qName.betaOnly`
  foreach item ( $sample )
    set url5="&db=$mouse&position=$item"
    echo "$urlbeta$url5"
  end
  tail -3 $mouse.igtc.qName.betaOnly
end


echo "\n\n----------------------"
echo "check out a few new alignments that hit on all 3 assemblies"
echo "(get the DNA and BLAT it to the others)"

rm -f allThree
foreach mouse ( $mice )
  cat $mouse.igtc.qName.devOnly >> allThree
end
sort allThree | uniq -c > allThree.sort
cat allThree.sort | awk '$1 == 3 {print;}' | head -2
cat allThree.sort | awk '$1 == 3 {print;}' | tail -2
echo

set first=`cat allThree.sort | awk '$1 == 3 {print $2;}' | head -1`
set last=`cat allThree.sort | awk '$1 == 3 {print $2;}' | tail -1`
foreach mouse ( $mice )
  set url2="&db=$mouse&position=$first"
  echo "$urldev$url2"
end
echo
foreach mouse ( $mice )
  set url3="&db=$mouse&position=$last"
  echo "$urldev$url3"
end


echo "\n\n----------------------"
echo "find items not in all assemblies.  do they simply not blat ? "
echo "number in two assemblies:" `cat allThree.sort | awk '$1 == 2 {print;}' \
  | wc -l`
echo "number in one assembly:  " `cat allThree.sort | awk '$1 == 1 {print;}' \
  | wc -l`

cat allThree.sort | awk '$1 == 2 {print;}' | head -1
cat allThree.sort | awk '$1 == 2 {print;}' | tail -1
cat allThree.sort | awk '$1 == 1 {print;}' | head -1
cat allThree.sort | awk '$1 == 1 {print;}' | tail -1
echo

set double=`cat allThree.sort | awk '$1 == 2 {print $2;}' | head -2`
set single=`cat allThree.sort | awk '$1 == 1 {print $2;}' | tail -2`

# if you want to make links to the whole list:
#  set double=`cat allThree.sort | awk '$1 == 2 {print $2;}'`
#  set single=`cat allThree.sort | awk '$1 == 1 {print $2;}'`

foreach item ( $double $single )
  set dbs=`grep $item *.igtc.qName.devOnly | awk -F. '{print $1}'`
  # the following may be used for getting the sequence to blat to the 
  # missing assembly.  then we'd know if it should be there really or 
  # not.  would be used with whole list.

  #  set missing="$mice"
  #  foreach mouse ( $dbs )
  #  set missing=`echo $missing | sed -e "s/ /\n/g" | grep -v $mouse`
  #  end
  #  echo "$item is missing from $missing"

  #  make links
  foreach db ( $dbs )
    set url4="&db=$db&position=$item"
    echo "$urldev$url4"
  end
  echo
end
echo


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
  hgsql -h hgwbeta -e 'SELECT * FROM extFile WHERE name LIKE "genetrap%"' \
     $mouse
  echo
end

# get the extFile.id value for this month's entry
echo "\n\n----------------------"
echo "You will need to know the id field from the extFile table entry"
echo "for this month's file (genetrap.$update.fasta):"

### ??? can't get the loop to work with $mice variable ??? ###
#foreach mouse ( $mice )
#  set extFileId_${mouse}=`hgsql -Ne "SELECT id FROM extFile WHERE name \
#    LIKE 'genetrap.$update.fasta'" $mouse`
#  echo "$mouse.extFile.id = $extFileId_$mouse"
#end

### So, I'll just do them one-by-one for now...
set extFileId_mm9=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE \
  'genetrap.$update%'" mm9`
echo "extFile.id_mm9 = $extFileId_mm9"

set extFileId_mm8=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE \
  'genetrap.$update%'" mm8`
echo "extFile.id_mm8 = $extFileId_mm8"

set extFileId_mm7=`hgsql -Ne "SELECT id FROM extFile WHERE name LIKE \
  'genetrap.$update%'" mm7`
echo "extFile.id_mm7 = $extFileId_mm7"


# compare counts with file counts
echo "\n\n----------------------"
echo "Sanity check: compare counts in seq table (on dev) with what's \
  in the file"
echo "(if the values aren't equal per assembly, this needs investigation)\n"

### ??? can't get the loop to work with $mice variable ??? ###
# So, just do it one-by-one for now...
echo "compare count from mm9.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm9'"' mm9
echo "to the count from mm9 genetrap.$update.fasta file:"
grep '>' /gbdb/mm9/igtc/genetrap.$update.fasta | wc -l
echo

echo "compare count from mm8.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm8'"' mm8
echo "to the count from mm8 genetrap.$update.fasta file:"
grep '>' /gbdb/mm8/igtc/genetrap.$update.fasta | wc -l
echo

echo "compare count from mm7.seq table:"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm7'"' mm7
echo "to the count from mm7 genetrap.$update.fasta file:"
grep '>' /gbdb/mm7/igtc/genetrap.$update.fasta | wc -l
echo


# get the new rows from tables on dev
echo "\n\n----------------------"
echo "Creating a file with the new rows from the extFile and seq tables \
  on dev"
echo "(these will need to be moved to beta)\n"

### ??? can't get the loop to work with $mice variable ??? ###
# So, just do it one-by-one for now...
hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm9'" mm9 \
  > mm9.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" \
  mm9 > mm9.extFile.$update

hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm8'" mm8 \
  > mm8.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" \
  mm8 > mm8.extFile.$update

hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId_mm7'" mm7 \
  > mm7.seq.$update
hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" \
  mm7 > mm7.extFile.$update


echo "The files are here:"
foreach mouse ( $mice)
  echo "$mouse.seq.$update"
  echo "$mouse.extFile.$update"
end

endif ##  not-run block


# set up deletion of the rows from beta tables.
echo "\n\n----------------------"
echo "Here are the commands needed to delete the rows from the extFile \
  and seq tables on DEV and BETA\n"
echo "(these are last month's data)"

# subtract one from the $month variable (so that it's last month's date)

set oldMonth=""
set oldYear=""
if ( $month == 01 ) then
  set oldMonth=12
  set oldYear=`echo $year | awk '{print $1-1}'`
else
  set oldMonth=`echo $month | awk '{print $1-1}'`
  if ( $oldMonth < 10 ) then
    set oldMonth='0'$oldMonth
  endif
  set oldYear=$year
endif

set lastMonth=$oldYear-$oldMonth-$day
echo "lastMonth = $lastMonth"

echo
echo "save existing tables"
echo

foreach mouse ( $mice )
  echo 'hgsql -e ' "'"CREATE TABLE extFile$oldYear${oldMonth}01 \
    SELECT \* FROM extFile"'" $mouse  
end
foreach mouse ( $mice )
  echo 'hgsql -e ' "'"CREATE TABLE seq$oldYear${oldMonth}01 \
    SELECT \* FROM seq"'" $mouse  
end
echo
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"CREATE TABLE extFile$oldYear${oldMonth}01 \
    SELECT \* FROM extFile"'" $mouse  
end
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"CREATE TABLE seq$oldYear${oldMonth}01 \
    SELECT \* FROM seq"'" $mouse  
end
echo

echo check:

foreach mouse ( $mice )
  echo 'hgsql -e ' "'"SHOW TABLES LIKE \"extFile%\" "'" \
    $mouse
end
foreach mouse ( $mice )
  echo 'hgsql -e ' "'"SHOW TABLES LIKE \"seq%\" "'" \
    $mouse
end
echo
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"SHOW TABLES LIKE \"extFile%\" "'" \
    $mouse
end
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"SHOW TABLES LIKE \" seq%\" "'" \
    $mouse
end


# Need to find the seq.id for last month's data
# (get from beta, as dev might not have it anymore)

set extFileIdOld_mm9=`hgsql -h hgwbeta -Ne 'SELECT id FROM extFile \
  WHERE name LIKE "genetrap.'$lastMonth%'"' mm9`
set extFileIdOld_mm8=`hgsql -h hgwbeta -Ne 'SELECT id FROM extFile \
  WHERE name LIKE "genetrap.'$lastMonth%'"' mm8`
set extFileIdOld_mm7=`hgsql -h hgwbeta -Ne 'SELECT id FROM extFile \
  WHERE name LIKE "genetrap.'$lastMonth%'"' mm7`

# setup deletion of the rows from tables on dev and beta
# commands needed to delete the rows from the extFile and seq tables on DEV and BETA
echo

foreach mouse ( $mice )
  echo 'hgsql -e ' "'"DELETE FROM $mouse.extFile WHERE name LIKE \
    '"'genetrap.$lastMonth.fasta'"' \' $mouse 
end

echo 'hgsql -e ' "'"DELETE FROM mm9.seq WHERE extFile = \
    '"'$extFileIdOld_mm9'"' \' mm9 
echo 'hgsql -e ' "'"DELETE FROM mm8.seq WHERE extFile = \
    '"'$extFileIdOld_mm8'"' \' mm8
echo 'hgsql -e ' "'"DELETE FROM mm7.seq WHERE extFile = \
    '"'$extFileIdOld_mm7'"' \' mm7

echo
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM $mouse.extFile WHERE name LIKE \
    '"'genetrap.$lastMonth.fasta'"' \' $mouse
end

echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM mm9.seq WHERE extFile = \
    '"'$extFileIdOld_mm9'"' \' mm9 
echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM mm8.seq WHERE extFile = \
    '"'$extFileIdOld_mm8'"' \' mm8
echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM mm7.seq WHERE extFile = \
    '"'$extFileIdOld_mm7'"' \' mm7


# is everything in seq?
echo "\n\n----------------------"
echo "Check that all igtc entries actually have an entry in seq:"
echo "Look for zero in the devOnly files."
echo

foreach mouse ( $mice )
  cat $mouse.seq.$update | awk '{print $2}' | sort > $mouse.seqNames
  commTrio.csh $mouse.igtc.qName.dev $mouse.seqNames   
end

echo "\n\n----------------------"
echo "How many items in seq still have old extFile number?"
echo "Should be more than the number in devOnly because some were not used"
echo " These should be lost using DELETE commands below)."
echo

echo "dev"
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm9'"' mm9
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm8'"' mm8
hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm7'"' mm7
wc -l *devOnly | grep -v total
echo
echo "beta"
hgsql -h hgwbeta -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm9'"' mm9
hgsql -h hgwbeta -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm8'"' mm8
hgsql -h hgwbeta -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm7'"' mm7


# load rows into tables on beta
echo "\n\n----------------------"
echo "Here are the commands needed to load the rows into extFile \
  and seq tables on BETA:\n"

foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"LOAD DATA LOCAL INFILE \
    '"'$mouse.seq.$update'"'     INTO TABLE seq"'"         $mouse
  echo 'hgsql -h hgwbeta -e ' "'"LOAD DATA LOCAL INFILE \
    '"'$mouse.extFile.$update'"' INTO TABLE extFile"'" $mouse
  echo
end

echo "\n\n----------------------"
echo "----------------------"
echo "other things to check/do:"
echo " - make sure the IGTC site links back to ours correctly"
echo " - push tables to hgwbeta:"
foreach mouse ( $mice )
  echo "     sudo mypush $mouse igtc hgwbeta"
end

echo " - ask for push of files from hgwdev to hgnfs1:"
foreach mouse ( $mice )
  echo "     /gbdb/$mouse/igtc/genetrap.$update.fasta"
end

echo " - ask for drop of old files from hgnfs1:"
foreach mouse ( $mice )
  echo "     /gbdb/$mouse/igtc/genetrap.$oldMonth.fasta"
end

echo
echo " - check that these new ones are ok on beta:"
echo
foreach mouse ( $mice )
  set url2="&db=$mouse&position=$first"
  echo "$urlbeta$url2"
end
echo
foreach mouse ( $mice )
  set url3="&db=$mouse&position=$last"
  echo "$urlbeta$url3"
end
echo

echo " - check for drops of old seq items"
echo "dev"
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm9'"' \' mm9
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm8'"' \' mm8
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm7'"' \' mm7
echo
echo "beta"
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm9'"' \' mm9
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm8'"' \' mm8
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld_mm7'"' \' mm7
echo

echo " - check for new seq items"
echo "dev"
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm9'"' \' mm9
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm8'"' \' mm8
echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm7'"' \' mm7
echo
echo "beta"
echo 'hgsql -h hgwbeta -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm9'"' \' mm9
echo 'hgsql -h hgwbeta -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm8'"' \' mm8
echo 'hgsql -h hgwbeta -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId_mm7'"' \' mm7
echo


echo "\nthe end.\n"

# clean up

exit 0

