#!/bin/tcsh


################################
#  06-15-04
#  Robert Kuhn
#
#  Runs joiner check, finding all identifiers for a table.
#
################################

set db=""
set table=""
set range=""
set joinerPath=""
# set joinerPath="~/schema"

if ($#argv < 2) then
  echo
  echo "  runs joiner check, -keys, finding all identifiers for a table."
  echo '  set database to "all" for global.'
  echo
  echo "    usage:  database, table, [path to all.joiner]"
  echo "           (defaults to tip of the tree)"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ($#argv == 3) then
  set joinerPath=$argv[3]
else
  # checkout tip of the tree
  if (! -d xxJoinDirxx) then
    mkdir xxJoinDirxx
  endif
  set joinerPath="xxJoinDirxx"
  setenv CVS_RSH ssh
  cvs -d hgwdev:/projects/compbio/cvsroot co -d xxJoinDirxx \
    kent/src/hg/makeDb/schema/all.joiner >& /dev/null

  if ( $status ) then
    echo
    echo "  cvs check-out failed for all.joiner on $HOST"
    echo
    exit 1
  endif

endif

if ($db != "all") then
  set range="-database=$db"
endif

# set joinerFile and eliminate double "/" where present
set joinerFile=`echo ${joinerPath}/all.joiner | sed -e "s/\/\//\//"`

# echo "joinerFile = $joinerFile"

# --------------------------------------------

# get identifiers
tac $joinerPath/all.joiner \
  | sed "/\.$table\./,/^identifier /\!d" | \
  grep "^identifier" | gawk '{print $2}' > xxIDxx

if (-e xxIDxx) then
  set idVal=`wc -l xxIDxx | gawk '{print $1}'`
  echo
  if ($idVal != 0) then
    echo "found identifiers:"
    echo
    cat xxIDxx
  else
    echo "  no identifiers for this table"
  endif
  echo
endif

foreach identifier (`cat xxIDxx`)
  joinerCheck $range -identifier=$identifier -keys $joinerFile 
end

rm -f xxIDxx
rm -r xxJoinDirxx 
