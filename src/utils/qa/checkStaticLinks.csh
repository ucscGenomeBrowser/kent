#!/bin/tcsh

###############################################
#  05-10-04
# 
#  checks the links in all the files in a directory
#  Robert Kuhn
# 
###############################################

set filePath=""
set yymmdd="today"
set exclude=""
set file=""
set currDir=$cwd

if ( $#argv < 1 || $#argv > 3 ) then
  # wrong number of command-line args
  echo
  echo "  checks the links in all the files in a directory on the RR."
  echo "    (uses directory on hgwbeta to get list)."
  echo
  echo "    usage:  pathIn/htdocs [yymmdd] [excludeList]"
  echo '      where:'
  echo '        pathIn/htdocs: (zero for root) '
  echo '        yymmdd: any dateString for output files. defaults to "today"'
  echo "        excludeList: filename or list of files not to check. "
  echo
  exit
endif

# strip trailing backslash"
if ($argv[1] == 0) then
  set filePath=""
else
  set filePath=`echo $argv[1] | sed -e 's/\/$//'`
endif

if ( $#argv > 1 ) then
  set yymmdd=$argv[2]
endif

if ( $#argv == 3 ) then
  set exclude=$argv[3]
  file $exclude | grep -q "ASCII text"
  if ( $status ) then
    echo "\nexclude file does not exist\n"
    exit 1
  else
    set exclude=`cat $exclude`
  endif
endif

# get list of active files from beta
# and strip off the pathnames from list leaving only filenames

set origlist=`ssh hgwbeta 'ls /usr/local/apache/htdocs/'${filePath}'/*html' \
      | sed -e "s/.*\///g"`

# echo "exclude = $exclude"
# strip out any files in exclude list
foreach excl ( $exclude )
  set origlist=`echo $origlist |  sed -e "s/ /\n/g" | egrep -v $excl`
end

echo $origlist | sed -e "s/ /\n/g" > ${currDir}/filelist

# echo "yymmdd = $yymmdd"
# echo "filepath = $filePath"
# echo $exclude

echo "files in htdocs/${filePath}"
cat filelist

echo

foreach file (`cat ${currDir}/filelist`)
  echo $file
  LinkCheck $argv[1] $file $yymmdd
end

rm -f linkCheck.all.$yymmdd
set outfile="linkCheck.all.$yymmdd"

echo >> $outfile
echo "========  reporting only on files with errors   ========" >> $outfile
echo "========================================================" >> $outfile
echo >> $outfile
dumpEmpty.csh .
foreach file (`ls -1 *.$yymmdd.errors`)
  # recover directory structure from dir.dir.dir.yymmdd.error files
  #   and strip out filename from any file with errors
  set filename=`echo $file | sed -e "s/\./\//g" | sed -e "s/\/$yymmdd\/errors/\.html/"`
  echo >> $outfile
  cat $file >> $outfile
  echo >> $outfile
  echo "========================================================" >> $outfile
  echo >> $outfile
# rm $file ??
end

rm -f filelist
