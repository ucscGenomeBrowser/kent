#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  05-12-04  Robert Kuhn
# 
#  checks all the static links in htdocs tree.
#  needs a way to re-check bad links. 
# 
###############################################

set filePath=""
set excludeList=""

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv != 1 ) then
  # wrong number of command line args
  echo
  echo "  checks all the static links in htdocs tree."
  echo "  uses directory on beta."
  echo "  excludes files listed in /cluster/bin/scripts/linkCheckExclude"
  echo
  echo '    usage: <fileOfPaths | all>'
  echo '       "all" uses /cluster/bin/scripts/staticpaths'
  echo
  exit
endif

if ($argv[1] == "all") then
  ## get all html-containing paths
  # find /usr/local/apache/htdocs -name "*.html" > htmlfiles
  # rm -f htmldirs
  # foreach file ( `cat htmlfiles` )
  #   dirname $file >> htmldirs
  # end
  # cat htmldirs | sort -u
  # rm -f htmlfiles

  # use default list of paths
  set pathfile="/cluster/bin/scripts/staticpaths"
else
  set pathfile=$argv[1]
  file $pathfile | grep -q "ASCII text"
  if ( $status ) then
    echo "\n file of paths $pathfile does not exist\n"
    exit 1
  endif
endif

set excludeList=/cluster/bin/scripts/linkCheckExclude
if ( $status ) then
  echo "\n exclude file does not exist\n"
  exit 1
endif

foreach filePath (`cat $pathfile`)
  echo "filePath: $filePath"
  echo "excludeList: $excludeList"
  checkStaticLinks.csh $filePath $excludeList
  if ( $status ) then
    echo "\n exclude file does not exist\n"
    exit 1
  endif
end
