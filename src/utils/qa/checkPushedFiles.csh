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
set website=''

if ( $#argv != 2 ) then
  echo
  echo " checks to see if files are in place, after a push"
  echo
  echo " usage: website files(s)"
  echo
  echo " website should include the path of the directory where"
  echo " the files reside, such as:"
  echo "   http://hgdownload.soe.ucsc.edu/goldenPath/hg19/liftOver/ "
  echo
  echo " file(s) is either a single name or a list of names, and can"
  echo " include items with additional directory structure, like so:"
  echo "   filename"
  echo "   dir/filename"
  echo "   dir/dir/dir/filename"
  echo
  echo " any output other than '200 OK' indicates an error."
  echo
  exit 1
else
  set website=$argv[1]
  set fileList=$argv[2]
endif

# check to see if it is a single filepath or a fileList
file $fileList | egrep -q "ASCII"
if (! $status) then
  set files=`cat $fileList`
else
  set files=$fileList
endif

foreach file ( $files )
  echo ${file}:
  wget -nv --spider $website$file
end

exit 0
