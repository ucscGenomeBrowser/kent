#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  05-10-2004
# 
#  checks the links in all the files in a directory
#  Robert Kuhn
# 
###############################################

set filePath=""
set out=""
set url=""
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
  set filePath=`echo $argv[1] | sed 's@/$@@'`
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

# strip out any files in exclude list
foreach excl ( $exclude )
  set origlist=`echo $origlist | sed "s/ /\n/g" | egrep -wv $excl`
end

# echo $origlist

# set up outfile for all the files in the dir
set i=0
rm -f outfile
echo "\nfiles checked in htdocs/${filePath}" >> outfile
echo $origlist | sed "s/ /\n/g"              >> outfile
echo                                         >> outfile

foreach file ( $origlist )
  rm -f tmp0
  htmlCheck checkLinks $baseUrl/$filePath/$file  >>& tmp0
  if ( -e tmp0 ) then
    # there were errors
    # clean out things we don't care about
    rm -f tmp
    cat tmp0 | grep -v "403" \
      | grep -v "doesn't exist" \
      | grep -v "Cancelling" \
      | grep -v "service not known" \
      | grep -v "than directories in" \
      | grep -v "Connection refused" \
      | egrep "."  > tmp
    rm -f tmp0

    if ( `wc -l tmp | awk '{print $1}'` > 0 ) then
      # there were errors worth looking at
      # get the link names for any broken urls
      @ errs = $errs + 1                    # counts files with errors
      set j=1
      set errors=`wc -l tmp | awk '{print $1}'`  # counts errs in file
      rm -f outfile$file
      echo                                           >> err$file
      while ( $j <= $errors )
        set errLine=`sed -n "${j}p" tmp`
        set url=`sed -n "${j}p" tmp | awk '{print $NF}'`
        set xfile=$baseUrl/$filePath/$file
        # set xfile=http://genome.ucsc.edu/goldenPath/credits.html
        # set url=http://www.genome.washington.edu/UWGC

        # grab 3 lines from html page and trim down to </A> tag
        set link=`htmlCheck getHtml $xfile | egrep -qi -A 4 "$url" \
          | sed -n "1,/<\/A>/p"`
        set link=`echo $link \
          | awk -F'</A>' '{print $1}' \
          | awk -F'>' '{print $NF}'`

        echo "link  = $link"                         >> err$file
        echo "error = $errLine"                      >> err$file
        echo                                         >> err$file
        @ j = $j + 1
      end
      @ j = $j - 1
      if ( $j > 0 ) then
        echo $file                                >> outfile$file
        echo $baseUrl/$filePath/$file             >> outfile$file
        cat err$file                              >> outfile$file
        echo " found $j errors in $file"          >> outfile$file
        echo "---------------------------"        >> outfile$file
        echo                                      >> outfile$file
        cat outfile$file                       >> outfile
      endif
      rm -f err$file
    endif
    rm -f tmp
    rm -f outfile$file
  endif
  @ i = $i + 1
end

echo "\n directory = htdocs/$filePath"         >> outfile
echo " checked $i files"                       >> outfile
# note:  if you change the line below the wrapper script will break
echo " found errors in $errs files\n"          >> outfile
echo                                           >> outfile

# cat outfile
if ( $filePath == "" ) then
  set out=htdocs.err
else
  set out=`echo $filePath | sed s@/@.@g`.err
endif
mv outfile $out

