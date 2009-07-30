#!/bin/tcsh
source `which qaConfig.csh`

################################
#  07-30-2009
#  makes sure the recently-pused files actually
#  made it to their destination
#
#  Ann Zweig
################################

set fileList=''
set files=''
set machine='hgdownload.cse.ucsc.edu'

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  checks to see if files are in place, after a push"
  echo
  echo "    usage: filepath [machine]"
  echo
  echo " include full web filepath like: /goldenPath/hg19/liftOver/md5sum.txt"
  echo "  accepts a single file or a file of filepaths\n"
  echo " machine defaults to hgdownload (type 'rr' for the public website)\n"
  echo " any output other than '200 OK' indicates an error.\n"
  exit
else
  set fileList=$argv[1]
  if ( 2 == $#argv ) then
    set machine=$argv[2]
  endif
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( 'rr' == $machine || 'RR' == $machine ) then
  set machine="genome.ucsc.edu"
endif

# check to see if it is a single filepath or a fileList
file $fileList | egrep -q "ASCII"
if (! $status) then
 set files=`cat $fileList`
else
 set files=$fileList
endif

foreach file ( $files )
  wget -nv --spider $machine$file
end

exit 0
