#!/bin/tcsh

###############################################
#  05-12-04
# 
#  checks all the static links in htdocs tree.
#  Robert Kuhn
# 
###############################################

set filePath=""
set yymmmdd="today"
set file=""
set currDir=$cwd

if ($#argv == 0) then
  # no command line args
  echo
  echo "  checks all the static links in htdocs tree."
  echo
  echo '    usage: <file of paths | all> -- "all" uses /cluster/bin/scripts/staticpaths,'
  echo '       yymmmdd (or other date string  --  defaults to "today")'
  echo
  exit
else
  if ($argv == "all") then
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
  #  ~/bin/checkStaticLinks.csh $filePath $yymmmdd
end
