#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  05-10-04
# 
#  checks the links in all the files in a directory
#  Robert Kuhn
# 
###############################################

set filePath=""
set out=""
set exclude=""
set excludeList=""
set baseUrl="http://hgwbeta.cse.ucsc.edu"
set errs=""

if ( $#argv < 1 || $#argv > 2 ) then
  # wrong number of command-line args
  echo
  echo "  checks the links in all the static pages in a directory."
  echo "  operates on pages on hgwbeta"
  echo "  writes a file called dir.dir.err"
  echo
  echo "    usage:  pathInHtdocs [excludeList]"
  echo '      where:'
  echo '        pathInHtdocs = path in htdocs (0 for htdocs root)'
  echo "        excludeList = filename for list of files not to check"
  echo
  exit
endif

if ($argv[1] == 0) then
  # filePath is already htdocs root level
else
  # strip trailing backslash"
  set filePath=`echo $argv[1] | sed -e 's/\/$//'`
endif

if ( $#argv == 2 ) then
  set excludeList=$argv[2]
  file $excludeList | grep -q "ASCII text"
  if ( $status ) then
    echo "\nexclude file $excludeList does not exist\n"
    exit 1
  endif
  set exclude=`cat $excludeList`
endif

# get list of active files from beta
# and strip off the pathname from list leaving only filenames

set origlist=`ssh hgwbeta 'ls /usr/local/apache/htdocs/'${filePath}'/*html' \
      | sed "s/.*\///g"`
echo

# strip out any files in exclude list
foreach excl ( $exclude )
  set origlist=`echo $origlist | sed "s/ /\n/g" | egrep -wv $excl`
end

set i=0
set errs=0
rm -f outfile
echo "\nfiles checked in htdocs/${filePath}" >> outfile
echo $origlist | sed "s/ /\n/g" >> outfile
echo >> outfile

foreach file ( $origlist )
  rm -f outfile$file
  echo $file                                    >>& outfile$file
  echo                 $baseUrl/$filePath/$file >>& outfile$file
  htmlCheck checkLinks $baseUrl/$filePath/$file >>& outfile$file
  if ( `cat outfile$file | grep -v "doesn't exist" | wc -l` > 2 ) then
    # there are errors
    cat outfile$file | grep -v "doesn't exist"  >> outfile
    echo                       >> outfile
    @ errs = $errs + 1
  endif
  @ i = $i + 1
  rm -f outfile$file
end
echo "\n directory" = $filePath >> outfile
echo " checked $i files" >> outfile
# note:  if you change the line below the wrapper script will break
echo " found errors in $errs files\n" >> outfile

# cat outfile
if ( $filePath == "" ) then
  set out=htdocs.err
else
  set out=`echo $filePath | sed s@/@.@g`.err
endif
mv outfile $out

