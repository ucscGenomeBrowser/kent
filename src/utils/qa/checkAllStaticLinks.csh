#!/bin/tcsh

###############################################
#  05-12-04  Robert Kuhn
# 
#  checks all the static links in htdocs tree.
#  needs a way to re-check bad links. 
# 
###############################################

set filePath=""
set yymmdd="today"
set file=""
set currDir=$cwd



if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif



if ($#argv == 0 || $#argv > 2) then
  # wrong number of command line args
  echo
  echo "  checks all the static links in htdocs tree."
  echo "  uses directory on beta."
  echo "  excludes files listed in /cluster/bin/scripts/linkCheckExclude"
  echo
  echo '    usage: <file of paths | all> [yymmdd]'
  echo '       "all" uses /cluster/bin/scripts/staticpaths'
  echo '        yymmdd: any dateString for output files. defaults to "today"'
  echo
  exit
else
  if ($argv[1] == "all") then
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
  if ($#argv == 2) then
    set yymmdd=$argv[2]
  endif
endif

cp /cluster/bin/scripts/linkCheckExclude excludeList
foreach filePath (`cat $pathfile`)
  echo "filePath: $filePath"
  checkStaticLinks.csh $filePath $yymmdd excludeList
end
