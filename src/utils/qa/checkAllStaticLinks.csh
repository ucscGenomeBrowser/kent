#!/bin/tcsh

###############################################
#  05-12-04  Robert Kuhn
# 
#  checks all the static links in htdocs tree.
#  needs a way to re-check bad links. 
# 
###############################################

set filePath=""
set yymmmdd="today"
set file=""
set currDir=$cwd

if ($#argv == 0 || $#argv > 2) then
  # no command line args
  echo
  echo "  checks all the static links in htdocs tree."
  echo "  uses directory on beta."
  echo
  echo '    usage: <file of paths | all> -- "all" uses /cluster/bin/scripts/staticpaths'
  echo '       yymmmdd (or other date string  --  defaults to "today")'
  echo
  exit
else
  if ($argv[1] == "all") then
    # use default
    set pathfile="/cluster/bin/scripts/staticpaths"
  else
    set pathfile=$argv[1]
  endif
  if ($#argv == 2) then
    set yymmmdd=$argv[2]
  endif
endif

foreach filePath (`cat $pathfile`)
  echo "filePath: $filePath"
  checkStaticLinks.csh $filePath $yymmmdd
end
