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
set counter=( 1 2 3 )
set init=( 0 0 0 )
set mice=( mm9 mm8 mm7 )
set extFileId=( $init )
set extFileIdOld=( $init )

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
  echo "  Script should be run before pushing new tables to hgwbeta."
  echo "  Warning: this script creates many files."
  echo
  exit
else
  set update=$argv[1]
endif


# check to see if input was even a date
echo $update | grep -q "^20"
if ($status ) then
  echo
  echo "  need to provide a valid date:  yyyy-mm-dd."
  echo
  exit 1
endif

# parse the date
set year=`echo $update | awk -F- '{print $1}'`
set month=`echo $update | awk -F- '{print $2}'`
set day=`echo $update | awk -F- '{print $3}'`

if ( 0 == 0 ) then  ##  not-run block

# make sure they have entered something close to correct date (kludge warning!)
if ( $year < 2007 || $year > 2020 || $month > 12 || $day > 31 ) then
  echo "\nERROR: the 'date' format is incorrect"
  $0
  exit 1
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\nERROR: you must run this script on hgwdev!\n"
  exit 1
endif

# Explain what we're QAing
echo "\nRunning on these mouse assemblies:"
echo "  $mice"
echo "\nAnd on these files:"

# make sure the files exist
foreach mouse ( $mice )
  set err=0
  if (! -e /gbdb/$mouse/igtc/genetrap.$update.fasta) then
    echo "ERROR: can't find the file: \
      /gbdb/$mouse/igtc/genetrap.$update.fasta"
    set err=1
  else
    echo "  /gbdb/$mouse/igtc/genetrap.$update.fasta"
  endif
end 
if ( $err == 1 ) then
  echo "\n Error.  Quitting \n"
  exit 1
endif
echo

echo "\n\n----------------------"
echo "run Joiner Check"
# runs on all assemblies
joinerCheck -keys -identifier=igtcName ~/kent/src/hg/makeDb/schema/all.joiner

echo "\n\n----------------------"
echo "run featureBits"
foreach mouse ( $mice )
  runBits.csh $mouse igtc
end

echo "\n\n----------------------"
echo "compare new (dev) and old (beta) tables"
echo
foreach mouse ( $mice )
  compareWholeColumn.csh $mouse igtc qName
end

echo "\n\n----------------------"
echo "check for number from all contributing labs."
foreach mouse ( $mice )
  # count up for each source
  echo
  echo $mouse
  cat $mouse.igtc.qName.dev | awk -F_ '{print $NF}' | sort | uniq -c \
    > $mouse.dev.out
  echo
  cat $mouse.igtc.qName.beta | awk -F_ '{print $NF}' | sort | uniq -c \
    > $mouse.beta.out
  # make sure the lists are the same and add zero record if missing
  set outList=`cat $mouse.dev.out $mouse.beta.out | awk '{print $2}' | sort -u`
  foreach source ( $outList)
    foreach mach ( dev beta )
      grep -q  $source $mouse.$mach.out
      if ( $status ) then
        echo "0 $source" >> $mouse.$mach.out
      endif
    end
  end
  sort -k2,2  $mouse.dev.out > $mouse.dev.out2
  sort -k2,2  $mouse.beta.out > $mouse.beta.out2

  echo "source hgwdev hgwbeta" \
    | awk '{printf("%-6s %6s %6s\n", $1, $2, $3)}'
  echo "------ ------ -------" \
    | awk '{printf("%-6s %6s %6s\n", $1, $2, $3)}'
  join -j2 $mouse.dev.out2 $mouse.beta.out2 \
    | awk '{printf("%-6s %6s %6s\n", $1, $2, $3)}'
  echo
  rm -f $mouse.*.out
  rm -f $mouse.*.out2
end

echo "\n\n----------------------"
echo "check a few of the new additions to the igtc table"
echo "(be sure to click all the way out to IGTC website)"
foreach mouse ( $mice )
  echo " \n\ncheck these in $mouse browser on hgwdev (or use links below):"
  head -3 $mouse.igtc.qName.devOnly
  set sample=`head -3 $mouse.igtc.qName.devOnly`
  foreach item ( $sample )
    set url6="&db=$mouse&position=$item"
    echo "$urldev$url6"
  end
  tail -3 $mouse.igtc.qName.devOnly
  set sample=`tail -3 $mouse.igtc.qName.devOnly`
  foreach item ( $sample )
    set url6="&db=$mouse&position=$item"
    echo "$urldev$url6"
  end
end

echo "\n\n----------------------"
echo "here's a list of the types of items being deleted from the igtc table"
echo "expect blank if the items are not deleted from the table"
foreach mouse ( $mice )
  echo "from $mouse"
  cat $mouse.igtc.qName.betaOnly | awk -F"_" '{print $NF}' | sort | uniq -c \
    | sort
end

echo "\n\n----------------------"
echo "check a few of the deleted items from the igtc table"
echo "(make sure they have also been dropped from IGTC website)"
echo "expect blank if the items are not deleted from the table"
foreach mouse ( $mice )
  echo " \n\ncheck these in $mouse browser on hgwbeta (or use links below):"
  head -3 $mouse.igtc.qName.betaOnly 
  set sample=`head -3 $mouse.igtc.qName.betaOnly`
  foreach item ( $sample )
    set url5="&db=$mouse&position=$item"
    echo "$urlbeta$url5"
  end
  echo
  echo "bottom of list:"
  tail -3 $mouse.igtc.qName.betaOnly
  set sample=`tail -3 $mouse.igtc.qName.betaOnly`
  foreach item ( $sample )
    set url5="&db=$mouse&position=$item"
    echo "$urlbeta$url5"
  end
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
# I think that this is not working properly -- it is miscalculating single hits
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
  echo "             hgwbeta:"
  hgsql -h hgwbeta -Ne 'SELECT * FROM extFile WHERE name LIKE "genetrap%"' \
     $mouse
  echo
end

# get the extFile.id value for this month's entry
echo "\n\n----------------------"
echo "Confirm that these match the new ids in the tables above"
echo "for this month's file (genetrap.$update.fasta):"
echo

foreach i ( $counter )
  set extFileId[$i]=`hgsql -Ne "SELECT id FROM extFile WHERE name \
    LIKE 'genetrap.$update.fasta'" $mice[$i]`
  echo "$mice[$i].extFileId = $extFileId[$i]"
end

# compare counts with file counts
echo "\n\n----------------------"
echo "Sanity check: compare counts in seq table (on dev) with what's \
  in the file"
echo "(if the values aren't equal per assembly, this needs investigation)\n"

echo "compare count from seq tables"
echo "to the count from genetrap.$update.fasta file:"
echo "will also be the same for all mice (see below for confirmation)"
echo "  - everything is there whether it aligns or not"

foreach i ( $counter )
  echo $mice[$i]
  hgsql -Ne 'SELECT COUNT(*) FROM seq \
    WHERE extFile = "'$extFileId[$i]'"' $mice[$i]
  grep '>' /gbdb/$mice[$i]/igtc/genetrap.$update.fasta | wc -l
echo
end

# check that all acc in the seq file are the same for all assemblies
echo "check that all acc in the seq file are the same for all assemblies"
foreach i ( $counter ) 
  echo $mice[$i]
  echo $extFileId[$i]
  hgsql -N -e "SELECT acc FROM seq WHERE extFile = $extFileId[$i]" $mice[$i] \
     > $mice[$i].seq.acc
end
echo
commTrio.csh $mice[1].seq.acc $mice[2].seq.acc
commTrio.csh $mice[2].seq.acc $mice[3].seq.acc


# get the new rows from tables on dev
echo "\n\n----------------------"
echo "Creating files with the new rows from the extFile and seq tables \
  on dev"
echo "(these rows will need to be moved to the approriate table on beta)\n"

foreach i ( $counter )
  hgsql -Ne "SELECT * FROM seq WHERE extFile = '$extFileId[$i]'" $mice[$i] \
    > $mice[$i].seq.$update
  hgsql -Ne "SELECT * FROM extFile WHERE path LIKE '%genetrap.$update.fasta'" \
    $mice[$i] > $mice[$i].extFile.$update
end

echo "The files are here:"
foreach i ( $counter )
  echo "$mice[$i].seq.$update"
  echo "$mice[$i].extFile.$update"
end

endif ##  not-run block


# set up deletion of the rows from beta tables.
echo "\n\n----------------------"
echo "Here are the commands needed to delete the old rows from the extFile \
  and seq tables on DEV and BETA\n"
echo "(these are last month's data)"

# get last month's date 
set lastMonth="`getLastMonth.csh go`-01"
set oldMonth=`echo $lastMonth | awk -F- '{print $2}'`
set oldYear=`echo $lastMonth | awk -F- '{print $1}'`
echo "lastMonth = $lastMonth"

echo
echo "save existing tables:"

foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -e ' "'"CREATE TABLE $table$oldYear${oldMonth}01 \
      SELECT \* FROM $table"'" $mouse  
  end
end
echo
foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -h hgwbeta -e ' "'"CREATE TABLE $table$oldYear${oldMonth}01 \
      SELECT \* FROM $table"'" $mouse  
  end
end
echo


echo check:
foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -e ' "'"SHOW TABLES LIKE \"$table%\" "'" \
      $mouse
  end
end
echo
foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -h hgwbeta -e ' "'"SHOW TABLES LIKE \"$table%\" "'" \
      $mouse
  end
end

echo
echo "drop previous month backup tables:"

set lastLastMonth="`getLastMonth.csh $oldYear-$oldMonth`"
set oldOldMonth=`echo $lastLastMonth | awk -F- '{print $2}'`
set oldOldYear=`echo $lastLastMonth | awk -F- '{print $1}'`
# echo "lastMonth = $lastMonth"
# echo "oldMonth = $oldMonth"
# echo "oldYear = $oldYear"
# echo
# echo "lastLastMonth = $lastLastMonth"
# echo "oldOldMonth = $oldOldMonth"
# echo "oldOldYear = $oldOldYear"

foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -e ' "'"DROP TABLE $table$oldOldYear${oldOldMonth}01"'" \
      $mouse
  end
end
echo
foreach table ( extFile seq )
  foreach mouse ( $mice )
    echo 'hgsql -h hgwbeta -e ' \
      "'"DROP TABLE $table$oldOldYear${oldOldMonth}01"'" \
      $mouse
  end
end
echo


# Need to find the seq.id for last month's data
# (get from beta, as dev might not have it anymore)

echo "remove old rows:"
foreach i ( $counter )
  set extFileIdOld[$i]=`hgsql -h hgwbeta -Ne 'SELECT id FROM extFile \
    WHERE name LIKE "genetrap.'$lastMonth%'"' $mice[$i]`
end

foreach mouse ( $mice )
  echo 'hgsql -e ' "'"DELETE FROM $mouse.extFile WHERE name LIKE \
    '"'genetrap.$lastMonth.fasta'"' \' $mouse 
end
foreach i ( $counter )
  echo 'hgsql -e ' "'"DELETE FROM $mice[$i].seq WHERE extFile = \
      $extFileIdOld[$i]\' $mice[$i]
end

echo
foreach mouse ( $mice )
  echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM $mouse.extFile WHERE name LIKE \
    '"'genetrap.$lastMonth.fasta'"' \' $mouse 
end
foreach i ( $counter )
  echo 'hgsql -h hgwbeta -e ' "'"DELETE FROM $mice[$i].seq WHERE extFile = \
      $extFileIdOld[$i]\' $mice[$i]
end
echo

# is everything in seq?
echo "\n\n----------------------"
echo "Check that all igtc entries actually have an entry in seq:"
echo "This may duplicate some of joinerCheck."
echo "Look for zero in the devOnly files."
echo

foreach mouse ( $mice )
  cat $mouse.seq.$update | awk '{print $2}' | sort > $mouse.seqNames
  commTrio.csh $mouse.igtc.qName.dev $mouse.seqNames   
end

echo "\n\n----------------------"
echo "How many items in seq still have old extFile number?"
echo "Should be more than the number in devOnly because some were not used"
echo "These should be lost using DELETE commands above)."
echo

echo "dev"
foreach i ( $counter )
  echo "$mice[$i] seq"
  hgsql -Ne 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld[$i]'"' \
    $mice[$i]
end
wc -l *devOnly | grep -v total
echo
echo "beta"
foreach i ( $counter )
  hgsql -h hgwbeta -Ne 'SELECT COUNT(*) FROM seq \
    WHERE extFile = "'$extFileIdOld[$i]'"' $mice[$i]
end
wc -l *betaOnly | grep -v total


# load rows into tables on beta
echo "\n\n----------------------"
echo "Here are the commands needed to load the new rows into extFile \
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

echo " - check push:"
foreach mouse ( $mice )
  echo "     realTime.csh $mouse igtc"
end


echo " - ask for push of files from hgwdev to hgnfs1:"
foreach mouse ( $mice )
  echo "     /gbdb/$mouse/igtc/genetrap.$update.fasta"
end

echo
echo " - check that these new ones are ok on beta:"
foreach mouse ( $mice )
  set url2="&db=$mouse&position=$first"
  echo "$urlbeta$url2"
end
foreach mouse ( $mice )
  set url3="&db=$mouse&position=$last"
  echo "$urlbeta$url3"
end
echo

echo " - check for drops of old seq items:"
foreach i ( $counter )
  echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld[$i]'"' \' $mice[$i]
end

foreach i ( $counter )
  echo 'hgsql -h hgwbeta -Ne' "'" \
    'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileIdOld[$i]'"' \' \
    $mice[$i]
end
echo

echo " - check for new seq items:"
foreach i ( $counter)
  echo 'hgsql -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId[$i]'"' \' $mice[$i]
end
foreach i ( $counter)
  echo 'hgsql -h hgwbeta -Ne' "'" 'SELECT COUNT(*) FROM seq WHERE extFile = "'$extFileId[$i]'"' \' $mice[$i]
end
echo
echo

echo " - ask for push of these tables: igtc, seq, extFile"
echo "     in mm7, mm8, mm9"
echo "   from hgwbeta -> RR"

echo " - ask for drop of old files from hgnfs1"
echo "   (do this AFTER previous table push is complete):"
foreach mouse ( $mice )
  echo "     /gbdb/$mouse/igtc/genetrap.$lastMonth.fasta"
end

echo " - Add reviewer name and qId (taken from queue Id at the top of current pushQ entry)"
echo " to notes section of pushQ entry" 

echo " - Paste featureBits output into Feature Bits section of pushQ entry"

echo "\nthe end.\n"

# clean up

exit 0

