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
set exclude=""
set excludeList=""
set file=""
set baseUrl="http://hgwbeta.cse.ucsc.edu"

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
  set file=0
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

# echo origlist before $origlist | sed "s/ /\n/g"
# echo "exclude = $exclude" | sed "s/ /\n/g"

# strip out any files in exclude list
foreach excl ( $exclude )
  set origlist=`echo $origlist | sed "s/ /\n/g" | egrep -v $excl`
end

# echo origlist after  $origlist | sed "s/ /\n/g"

set i=0
rm -f outfile$i
echo "\nfiles checked in htdocs/${filePath}" >> outfile$i
echo $origlist | sed "s/ /\n/g" >> outfile$i
echo >> outfile$i

foreach file ( $origlist )
  rm -f outfile$file
  echo $file                                    >>& outfile$file
  echo                 $baseUrl/$filePath/$file >>& outfile$file
  htmlCheck checkLinks $baseUrl/$filePath/$file \
    |& grep -v "doesn't exist"                   >>& outfile$file
  if ( `cat outfile$file | wc -l` > 2 ) then
    cat outfile$i outfile$file  > outfileTmp
    echo                       >> outfileTmp
  else
    mv outfile$i outfileTmp
  endif
  rm -f outfile$i
  @ i = $i + 1
  if ( -e outfileTmp ) then
    mv outfileTmp outfile$i
  endif
  rm -f outfile$file
end
mv outfile$i outfile
echo "\nchecked $i files\n" >> outfile

cat outfile
if ( $filePath == "" ) then
  set file=htdocs.err
else
  set file=`echo $filePath | sed s@/@.@g`.err
endif
mv outfile $file
