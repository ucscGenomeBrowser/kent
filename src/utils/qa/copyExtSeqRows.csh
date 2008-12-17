#!/bin/tcsh

#################################
# 
# Ann Zweig -- 2008 
# 
# This script copies rows from the extFile and seq tables
# for a given database from hgwdev onto hgwbeta.
# It attempts to handle all cases: a new track or a data update, and in 
# the case of an update, the files can have the same or a new name. 
# 
#################################

set DEBUG=0 

set db=''
set dbs=''
set filelist=''
set files=''
set type=''
set run=''
set numTables=''
set case=0
set fileEmpty=' '
set seqRowsBeta=''
set seqRowsDev=''
set seqTable=''
set fileAge=240
set tableList=''
set today=`date +%Y%m%d%H%M`
set listOfFiles='XXsetupFileListXX XXextFileDropFromBetaXX XXseqDropFromBetaXX XXextFileCopyFromDevXX XXseqCopyFromDevXX XXcaseXX XXseqTableXX'
set extraFiles='XXextFileDropFromBetaRealXX'

# usage statement
if ( $#argv != 4 ) then
 echo
 echo " Automatically copies appropriate rows from the extFile and seq tables"
 echo " from hgwdev to hgwbeta.  (You may want to send an email to browser-qa"
 echo " telling them 'hands off' the extFile and seq tables for now.)"
 echo 
 echo "    usage: db [file | fileList] [new | update] [setup | real]"
 echo 
 echo " Accepts either one fileName or a file containing a list of fileNames."
 echo " Does not accept wildcards (* or %)."
 echo " Use entire path to file like so: /gbdb/db/.../fileName."
 echo " If this is a data update to a track, even if the file names are unique,"
 echo " run with 'update', else run with 'new'.\n"
 echo " This script must be run TWICE: first run with 'setup',"
 echo " then review the output, if it's OK, then run again with 'real'.\n"
 exit 1
else
 set db=$argv[1]
 set filelist=$argv[2]
 set type=$argv[3]
 set run=$argv[4]
endif

# run only from hgwdev
if ( "hgwdev" != "$HOST" ) then
 echo "\n ERROR: you must run this script from hgwdev!\n"
 exit 1
endif

# check out all input parameters
set dbs=`hgsql -e "SELECT name FROM dbDb" hgcentraltest | grep -w $db`
if ( "$dbs" != $db ) then
 echo "\n ERROR: First argument must be a database that exists on dev & beta\n"
 $0
 exit 1
endif

if ( "new" != $type && "update" != $type ) then
 echo "\n ERROR: Third argument must specify if this is 'new' data or a data"
 echo " 'update'. If the track already exists, and these data will write"
 echo " over what's there (even if the old/new file names are different)"
 echo " choose 'update'.  Otherwise, choose 'new'.\n"
 $0
 exit 1
endif

if ( "setup" != $run && "real" != $run ) then
 echo "\n ERROR: Fourth argument must specify if this is a setup run or a real"
 echo " run. Run it for the first time as 'setup', then review the output."
 echo " If everything looks okay, run it again as 'real'.\n"
 $0
 exit 1
endif

# check to see if it is a single fileName or a fileList
file $filelist | egrep "ASCII text" > /dev/null
if (! $status) then
 set files=`cat $filelist`
else
 set files=$filelist
endif

# if this is a "real" run, we don't want to remove these files
if ( 'setup' == $run ) then
 rm -f $listOfFiles
else
 # but we do want to remove this file in a "real" run
 rm -f XXsetupFileListXX
endif

# check to make sure the files they specified exist and the path contains 
# the $db name.  In some cases, the file might exist in the hgFixed database.
# NOTE: This is a potential problem in the rare case where 
# the db name (or hgFixed) is *not* embedded in the file path.
foreach oneFile ( $files )
 ls -l $oneFile | egrep "$db|hgFixed" > /dev/null
 if ( $status ) then
  echo "\n ERROR: one or more of the file name(s) you provided either doesn't"
  echo " exist or the file path doesn't contain the database name you"
  echo " provided. Are you sure you gave the correct file name and path?"
  echo "        Your database name: $db"
  echo "        Your bad file name: $oneFile\n"
  exit 1
 else
  echo $oneFile >> XXsetupFileListXX
 endif
end

### START SETUP RUN
if ( 'setup' == $run ) then
 # check that the fileName(s) end in either .maf, .fa or .fasta (very rare)
 foreach oneFile ( $files )
  ls $oneFile | egrep 'maf$|fa$|fasta$' > /dev/null
  if ( $status ) then
   echo "\n ERROR: one or more of the file(s) in your list are not of the"
   echo " expected type. Typical file types in the extFile table end in .maf"
   echo " or .fa (or somtimes .fasta)\n"
   echo "      Your bad file: $oneFile\n"
   exit 1
  endif
 end

 # check hgwbeta to see if the extFile row(s) exist there
 # if this is a data UPDATE, make file containing lists of what will be
 # dropped from hgwbeta for users review.
 # if the file is empty, then that means this must be NEW data.
 foreach oneFile ( $files )
  hgsql -h hgwbeta -Ne "SELECT * FROM extFile WHERE path = '$oneFile'" $db \
   >> XXextFileDropFromBetaXX 
 end

 # check to make sure that they chose correctly between 'new' and 'update'
 if ( 'new' == $type ) then
  set numRows=`cat XXextFileDropFromBetaXX | wc -l`
  if ( 0 != $numRows ) then
   echo "\n ERROR: Although you set the third argument to 'new' (not 'update')"
   echo " this doesn't appear to be a new track. There are $numRows entries"
   echo " in the extFile table on hgwbeta for this track.  Please"
   echo " double-check. If the track already exists on hgwbeta, (even if"
   echo " the old/new file names are different) run the script again,"
   echo " but choose 'update' (instead of 'new')."
   $0
   exit 1
  endif
 endif

 # get extFile.id of row(s) from extFile on hgwbeta
 set extFileId=`cat XXextFileDropFromBetaXX | awk '{print $1}'`

 # it is possible (even common) for there to be no seq table
 set seqTable=`hgsql -h hgwbeta -Ne "SHOW TABLES LIKE 'seq'" $db`
 if ( "" == $seqTable ) then
  set seqTable=0
  echo "0" > XXseqTableXX
 else
  set seqTable=1
  echo "1" > XXseqTableXX
 endif

 # create file of row(s) from seq on hgwbeta (if there is a seq table)
 if ( 1 == $seqTable ) then
  foreach id ( "$extFileId" )
   hgsql -h hgwbeta -Ne "SELECT * FROM seq WHERE extFile = '$id'" $db \
    >> XXseqDropFromBetaXX
  end
 else
  echo "empty" > XXseqDropFromBetaXX
 endif

 # see how many seq entries there are (it's possible that there are none)
 set seqRowsBeta=`cat XXseqDropFromBetaXX | grep -v 'empty' | wc -l`

 # there are two cases of UPDATE in which this track is replacing an 
 # existing track.:
 # Case I: the old and new file names are the same, or 
 # Case II: the old and new file names are different.
 # if the file is present but empty, then there were no rows in extFile on beta
 if ( 'update' == $type ) then
  set fileEmpty=`find XXextFileDropFromBetaXX -empty`
  if ( "" == "$fileEmpty" ) then
   set case=1
   echo "1" > XXcaseXX
  else
   set case=2
   echo "2" > XXcaseXX
  endif
 else
  echo "0" > XXcaseXX
 endif

 # create file of row(s) from extFile on hgwdev to copy to hgwbeta
 foreach oneFile ( $files )
  hgsql -Ne "SELECT * FROM extFile WHERE path = '$oneFile'" $db \
   >> XXextFileCopyFromDevXX
 end

 # get extFile.id of row(s) from extFile on hgwdev
 set extFileId=`cat XXextFileCopyFromDevXX | awk '{print $1}'`

 # create file of row(s) from seq on hgwdev to copy to hgwbeta
 # (if there is a seq table)
 if ( 1 == $seqTable ) then
  foreach id ( "$extFileId" )
   hgsql -Ne "SELECT * FROM seq WHERE extFile = '$id'" $db >> XXseqCopyFromDevXX
  end
 else
  echo "empty" > XXseqCopyFromDevXX
 endif
  
 # see how many seq entries there are (it's possible that there are none)
 set seqRowsDev=`cat XXseqCopyFromDevXX | grep -v 'empty' | wc -l`

 # display output for user's review
 echo "\nSUCCESS: The 'setup' run of this script has completed successfully!\n"
 echo " Now you must carefully review the output files.  If you determine that"
 echo " they are correct, run this script again, in the same directory, with"
 echo " the same parameters, substituting 'real' for 'setup'.\n"
 echo "1. Check these two files to see what the script proposes to copy"
 echo "  from the seq and extFile tables on hgwdev to those tables on hgwbeta"
 echo "  (these files are in the directory you ran this script in):\n"
 echo "     XXextFileCopyFromDevXX XXseqCopyFromDevXX\n"

 # if this is a data UPDATE, display list of what will be dropped:
 if ( "update" == $type ) then
  if ( 1 == "$case" ) then
   # Update Case I (file names the same)
   echo "2. Because the underlying data files have the same names on dev (new)"
   echo "  and beta (old) check the output files for a list of what the script"
   echo "  proposes to drop from hgwbeta (when you run it again with the 'real'"
   echo "  parameter). The files are in the directory you ran this script in:\n"
   echo "     XXextFileDropFromBetaXX XXseqDropFromBetaXX\n"
  else
   # Update Case II (file names different)
   echo "2. Because the underlying data files have different names on dev (new)"
   echo "  than they do on beta (old), YOU MUST DROP the rows from the extFile"
   echo "  and seq tables on beta YOURSELF *before* you run this script again"
   echo "  with *real*. If you skip this step, you are going to have a BIG MESS!\n"
  endif
 else
  echo "2. Given your input parameters, there is no step two!\n" 
 endif

 echo "3. It is possible (even common) for there to be no seq table at all."
 if ( 0 == $seqTable ) then
  echo "  For your database, there is no seq table on hgwbeta. Make sure"
  echo "  that makes sense to you before you run this script with 'real'."
  echo "  This script will work fine, even without a seq table.\n"
 else
  echo "  For your database, there is a seq table on hgwbeta. Make sure that"
  echo "  makes sense to you before you run this script with 'real'.\n"
 endif

 echo "4. It is possible that although there are entries in the extFile table,"
 echo "  there are no corresponding entries in the seq table for this track"
 echo "  (or even, no seq table at all). In your run, the seq table on dev has"
 echo "  $seqRowsDev rows that correspond to this track, and the seq table on"
 echo "  beta has $seqRowsBeta rows corresponding to this track. Make sure"
 echo "  this makes sense to you.\n"

 echo "5. The file(s) that you used as an input parameter to this script"
 echo "  (whether it was a single file, or a fileList), should match the files"
 echo "  listed in this file: XXsetupFileListXX.\n"

 echo "6. As an additional sanity check, you could compare the number of"
 echo "  items in your input file(s) to the count from the seq table on dev"
 echo "  which is $seqRowsDev (this only works for .fasta and .fa files, not"
 echo "  .maf files). You should expect them to be the same. Do not simply"
 echo "  compare the row counts of your file(s) with the $seqRowsDev rows in"
 echo "  the seq table, you will have to grep your files for actual item"
 echo "  entires (quite often the items are preceeded by a '>')." 
 echo "    e.g. cat /gbdb/../../fileName | grep '>' | wc -l\n"

### END SETUP RUN
endif

### START REAL RUN
if ( 'real' == $run ) then
 # make sure they actually ran the SETUP run (check for output)
 set filesInDir=`ls XX*`
 foreach file ( $listOfFiles )
  echo $filesInDir | grep -q $file
  if ( $status ) then
   echo "\n ERROR: can not find the files that should have been created by"
   echo " the *setup* run of this script.  Are you sure you already ran"
   echo " this script in 'setup' mode (from this directory)?\n"
   exit 1
  endif
 end

 # make sure they ran the SETUP run *recently* (the past 4 hours) otherwise,
 # things could have changed and that would be a big mess.
 set recentFiles=`find . -type f -cmin -$fileAge`
 foreach file ( $listOfFiles )
  echo $recentFiles | grep -q $file
  if ( $status ) then
   echo "\n ERROR: the files needed to complete this 'real' run are too old."
   echo " Start over: run the script again in 'setup' mode and re-review"
   echo " the output then, run it again in 'real' mode.\n"   
   exit 1
  endif
 end

 # if this is an 'update' run, find out if it's Case I or Case II
 # the XXcaseXX file is created during the 'setup' run and it
 # contains a value of 0 if this is not an update run (no Case).
 set case=`cat XXcaseXX`

 # Make sure the seq and extFile tables exist on beta (they 
 # shouldn't have come this far without them, but you never know).
 # The XXseqTableXX file contains a 0 if there is no seq table on beta
 set seqTable=`cat XXseqTableXX`
 if ( 0 == $seqTable ) then
  set tableList="extFile"
 else
  set tableList="extFile seq"
 endif
 foreach table ( $tableList )
  set numTables=`hgsql -h hgwbeta -Ne "SHOW TABLES LIKE '$table'" $db`
  if ( $numTables == '' ) then
   echo "\n ERROR: the $table table does not exist on hgwbeta."
   echo " You should create an empty $table table in the $db database"
   echo " on hgwbeta and start this script over from the beginning using"
   echo " the 'setup' parameter.\n"
   exit 1
  endif
 end

 if ( 1 == $DEBUG ) then 
  echo "This is a DEBUG run, these things are not really going to happen:\n"
 endif

 # Make a backup of seq and extFile tables on hgwbeta.
 foreach table ( $tableList )
  if ( 0 == $DEBUG ) then
   hgsql -h hgwbeta -e "CREATE TABLE IF NOT EXISTS $table$today \
    SELECT * FROM $table" $db
  else
   # DEBUG mode
   echo "hgsql -h hgwbeta -e "'CREATE TABLE IF NOT EXISTS '$table''$today' \
    SELECT * FROM '$table''" $db\n"
  endif
 end
 
 # drop this file if it exists (just in case they had difficulties, and
 # this is their second 'real' run)
 rm -f XXextFileDropFromBetaRealXX

 # Make sure the extFile table on beta hasn't changed since 'setup' was run.
 # If this is a Case II Update, we expect the table to have changed, so skip it.
 if ( 2 != $case ) then
  foreach oneFile ( $files )
   hgsql -h hgwbeta -Ne 'SELECT * FROM extFile WHERE path = "'$oneFile'"' \
    $db >> XXextFileDropFromBetaRealXX
  end

  set numDiffs=`diff XXextFileDropFromBetaXX XXextFileDropFromBetaRealXX \
   | wc -l`
  if ( 0 != $numDiffs ) then
   echo "\nERROR: The extFile table in the $db database on hgwbeta has changed"
   echo "  since you ran this script in 'setup' mode. This is a problem. The"
   echo "  rest of this script will not work. You should spend some time"
   echo "  figuring out why/how this happened, fix it, then start this script"
   echo "  over using 'setup'.\n"
   exit 1
  endif
 endif

 # if this is a data UPDATE, Case I, drop rows from $db.seq and $db.extFile on hgwbeta. 
 # If this is a data UPDATE, Case II, assume they dropped the rows themself (as instructed)!
 if ( "update" == $type && 1 == $case ) then
  # Make sure there's something to drop (it's OK for the seq file to be empty)
  set fileEmpty=`find XXextFileDropFromBetaXX -empty`
  if ( "" != "$fileEmpty" ) then
   echo "\nERROR: the file containing the list of rows to drop from the extFile"
   echo "  table on beta is empty.  This is quite suspicious.  If this is"
   echo "  truly a data update, and the file names on dev are the same as the"
   echo "  file names on beta, then there is a big problem.  You should stop"
   echo "  now and try to figure out exactly what's going on.\n"
   exit 1 
  else
   # Drop rows from hgwbeta: $db.extFile based on: XXextFileDropFromBetaXX
   # Get the list of id(s) of the rows to drop from the extFile table on beta
   set extFileId=`cat XXextFileDropFromBetaXX | awk '{print $1}'`
   if ( 0 == $DEBUG ) then
    foreach id ( $extFileId )
     hgsql -h hgwbeta -e "DELETE FROM extFile WHERE id = '$id'" $db
    end
   else
    # DEBUG mode
    echo "hgsql -h hgwbeta -e "'DELETE FROM extFile WHERE id = '$extFileId''" \
     $db\n"
   endif

   # Drop rows from hgwbeta: $db.seq based on: XXseqDropFromBetaXX
   # First make sure the file isn't empty (it's OK if it is)
   set fileEmpty=`cat XXseqDropFromBetaXX | grep -v 'empty' | wc -l`
   if ( 0 != "$fileEmpty" ) then
    # Drop rows from hgwbeta $db.seq based on XXseqDropFromBetaXX
    # get the unique extFile column numbers from the file:
    set seqExtFile=`cat XXseqDropFromBetaXX | awk '{print $5}' | sort -u`
    foreach val ( $seqExtFile )
     if ( 0 == $DEBUG ) then
      hgsql -h hgwbeta -e "DELETE FROM seq WHERE extFile = '$val'" $db
     else
      # DEBUG mode
      echo "hgsql -h hgwbeta -e "'DELETE FROM seq WHERE extFile = '$val''" $db\n"
     endif
    end #foreach
   endif
  endif
 endif

 # do this for all instances: new data, update Case I, update Case II
 # load data into hgwbeta: $db.extFile based on: XXextFileCopyFromDevXX
 if ( 0 == $DEBUG ) then
  hgsql -h hgwbeta -e 'LOAD DATA LOCAL INFILE "'XXextFileCopyFromDevXX'" \
   INTO TABLE extFile' $db
 else
  # DEBUG mode
  echo "hgsql -h hgwbeta -e "'LOAD DATA LOCAL INFILE 'XXextFileCopyFromDevXX' \
   INTO TABLE extFile'" $db\n"
 endif

 # load data into hgwbeta: $db.seq based on: XXseqCopyFromDevXX
 if ( 0 == $DEBUG ) then
  hgsql -h hgwbeta -e 'LOAD DATA LOCAL INFILE "'XXseqCopyFromDevXX'" \
   INTO TABLE seq' $db
 else
  # DEBUG mode
  echo "hgsql -h hgwbeta -e "'LOAD DATA LOCAL INFILE 'XXseqCopyFromDevXX' \
   INTO TABLE seq'" $db\n"
 endif

 # display final information for user:
 echo "\nSUCCESS: The 'real' run of this script has completed successfully!"
 echo " Now there are some things you need to know and do.\n"

 if ( "update" == $type ) then
  if ( 1 == "$case" ) then
   # update Case I
   echo "1. Because this is a data update and the underlying data files on"
   echo "  hgwdev have the same name as they do on hgwbeta, the rows in the"
   echo "  extFile and seq tables on hgwbeta were dropped automatically by this"
   echo "  script. These two files contain a list of what was automatically"
   echo "  dropped from the extFile and seq tables on hgwbeta:\n"
   echo "     XXextFileDropFromBetaXX XXseqDropFromBetaXX\n"
  else
   # update Case II
   echo "1. Because this is a data update and the underlying data files on"
   echo "  hgwdev have a different name than they do on hgwbeta, the script"
   echo "  assumed that you dropped the appropriate rows from extFile and seq"
   echo "  by hand.  If you did not do that before this real run of the"
   echo "  script, you probably have a BIG MESS to clean up!\n"
  endif
 else
  # new data
  echo "1. Becuase this is new data, this script did not drop anything from"
  echo "  the extFile and seq tables on hgwbeta.\n"
 endif

 echo "2. This script loaded the appropriate set of rows into the extFile"
 echo "  and seq tables on hgwbeta.  You can check these two files if you are"
 echo "  interested in seeing exactly what rows were loaded:\n"
 echo "     rows loaded into $db.extFile on hgwbeta: XXextFileCopyFromDevXX"
 echo "     rows loaded into $db.seq on hgwbeta: XXseqCopyFromDevXX\n"

 echo "3. In case things seem terribly wrong, here is some pseudocode that"
 echo "  will help you delete the data that was automatically loaded into the"
 echo "  extFile and seq tables on hgwbeta. If all is well, do NOT do this!\n"
 echo '#drop rows from $db.extFile:'
 echo '# set extFileId to this: cat XXextFileCopyFromDevXX | awk {print $1}'
 echo '# foreach id in $extFileId'
 echo '#  hgsql -h hgwbeta -e DELETE FROM extFile WHERE id = $id $db'
 echo '# end'
 echo
 echo '#drop rows from $db.seq:'
 echo '# set seqExtFile to this cat XXseqCopyFromDevXX | awk {print $5} | sort -u'
 echo '# foreach val in $seqExtFile'
 echo '#  hgsql -h hgwbeta -e DELETE FROM seq WHERE extFile = $val $db'
 echo '# end\n'

 echo "4. This script only *copied* rows from the tables on dev to those on beta;"
 echo "  it did not remove them from the tables on dev.  If you determine that"
 echo "  the rows should be removed from the extFile and seq tables on dev,"
 echo "  you will need to do that yourself.  You will find a list of the rows"
 echo "  that can be removed in these two files:\n"
 echo "     XXextFileCopyFromDevXX    XXseqCopyFromDevXX\n"

 echo "5. Before any changes were made on hgwbeta, the extFile and seq tables"
 echo "  were backed up.  If you are sure that everything went fine, you"
 echo "  should delete them (from hgwbeta):\n"
 echo "     $db.seq$today;"
 echo "     $db.extFile$today;\n"

 echo "6. Many files were created during the running of this script."
 echo "  When you're ready to do a cleanup, here's a list of what can be"
 echo "  deleted (it is OK if not all of the files exist):\n"
 echo "     $listOfFiles $extraFiles\n"

### END REAL RUN
endif

echo "\nthe end.\n"
exit 0
