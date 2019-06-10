#!/bin/tcsh
source `which qaConfig.csh`


################################
#  06-15-04
#  Robert Kuhn
#
#  Runs joiner check, finding all identifiers for a table.
#
################################

onintr cleanup

set db=""
set table=""
set range=""
set joinerFile="$HOME/kent/src/hg/makeDb/schema/all.joiner"
set noTimes=""

if ( $#argv < 2 || $#argv > 4 ) then
  echo
  echo "  runs joinerCheck -keys, finding all identifiers for a table."
  echo '  runs joinerCheck -times (use "noTimes" to disable).'
  echo '  set database to "all" for global.'
  echo '  for chains/nets, use tablename format: chainDb.'
  echo
  echo "    usage:  database table [all.joiner file to use] [noTimes]"
  echo "    (defaults to $HOME/kent/src/hg/makeDb/schema/all.joiner)"
  echo
  exit 1
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ( $#argv == 3 ) then
  set noTimes=$argv[3]
  if ( $noTimes != "noTimes" ) then
    set joinerFile=$argv[3]
    set noTimes=""
  endif
endif

if ( $#argv == 4 ) then
  set joinerFile=$argv[3]
  set noTimes=$argv[4]
  if ( $noTimes != "noTimes" ) then
    echo
    echo "${0}:"
    $0
    exit 1
  endif
endif

if ($db != "all") then
  set range="-database=$db"
endif


# --------------------------------------------

# get identifiers

# use chain identifier if table is netDb#
echo $table | grep "^net" >& /dev/null
if ( $status == 0 ) then
  set table=`echo $table | sed -e "s/net/chain/"`
endif

# check for chain identifiers
echo $table | grep "chain" >& /dev/null
if ( $status == 0 ) then
  echo "\nchain and net use same identifier"
  echo $table | grep "chainSelf" >& /dev/null
  if ( $status == 0 ) then
    echo ${table} > xxIDxx
  else
    echo ${table}Id > xxIDxx
  endif
else
  # set non-chain identifiers
  tac $joinerFile \
    | sed "/\.$table\./,/^identifier /\!d" | \
    grep "^identifier" | gawk '{print $2}' > xxIDxx
  if ( $status ) then
   # if no identifier, look for whether table is ignored
    echo
    tac $joinerFile \
      | sed "/$table/,/^tablesIgnored/\!d" | \
      grep "^tablesIgnored"
    if ( $status ) then 
      echo "\n  Identifier not found, and not in tablesIgnored: $table"
    endif

    rm -f xxIDxx
    echo
  exit 1
  endif
endif


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
  echo "\n identifier= $identifier"
  nice joinerCheck $range -identifier=$identifier -keys $joinerFile 
end

if ( $noTimes == "" ) then
  echo "running -times flag\n"
  nice joinerCheck $range -times $joinerFile
endif

cleanup:
rm -f xxIDxx

exit 0
