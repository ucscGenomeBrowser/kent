#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  11-17-06
#  Robert Kuhn
#
#  checks an entire db between two nodes in realTime
#
################################

set db=""
set active=""
set mach1="hgwbeta"
set mach2="hgw1"
set url1=""
set url2=""
set url=""
set times=0

if ( $#argv < 1 || $#argv > 4 ) then
  echo
  echo "  checks on table match for an entire db between two nodes in realTime."
  echo "  optionally reports if update times do not match."
  echo "  ignores genbank tables."
  echo "  not in real time for non-assembly databases."
  echo
  echo "    usage:  database [machine1 machine2] [times]"
  echo "              defaults to beta and hgw1"
  echo
  exit
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv == 2 ) then
  if ( "times" == $argv[2] ) then
    set times=1
  else
    echo
    echo " you must specify two machines if you specify one"
    echo
    echo ${0}:
    $0
    exit 1
  endif
endif

if ( $#argv == 3 || $#argv == 4 ) then
  set mach1=$argv[2]
  set mach2=$argv[3]
endif

checkMachineName.csh $mach1 $mach2
if ( $status ) then
  exit 1
endif

if ( $#argv == 4 ) then
  if ( $argv[4] == "times" ) then
    set times=1
  else
    echo
    echo ' the fourth argument must be "times"\n'
    echo ${0}:
    $0
    exit 1
  endif
endif

# get tablenames for stripping out genbank
rm -f genbank.local$$
cat $GENBANK | sed -e 's/^^//; s/.$//' > genbank.local$$
echo gbLoaded >> genbank.local$$

# 
set active=`hgsql -h $sqlbeta -Ne "SELECT name FROM dbDb where active = 1" hgcentralbeta \
  | grep -w $db`
foreach machine ( $mach1 $mach2 )
  # check if db is for active assembly on beta
  # (if not, can't do real-time check)
  if ( $db == $active ) then
    # get the full table status in real time
    getTableStatus.csh $db $machine > $machine.tmp$$
    if ( $status ) then
      cat $machine.tmp$$
      rm $machine.tmp$$

      exit 1
    endif
  else
    # db is either not active, or not an assembly.  must use status dumps.
    cp `getRRdumpfile.csh $db $machine` $machine.tmp$$
  endif
  
  # drop header lines from file and grab appropriate fields
  if ( 1 == $times) then
    # find mysql version before grabbing STATUS fields
    set ver=`getVersion.csh $machine 1`
    set subver=`getVersion.csh $machine 2`
    if ( 4 == $ver && 1 == $subver || 5 == $ver ) then
      # newer mysql versions use different fields
      cat $machine.tmp$$ | sed '1,2d' \
        | awk '{print $1, $14, $15}' >& $machine.status$$
    else
      cat $machine.tmp$$ | sed '1,2d' \
        | awk '{print $1, $13, $14}' >& $machine.status$$
    endif
  else
    cat $machine.tmp$$ | sed '1,2d' \
      | awk '{print $1}' >& $machine.status$$
  endif
  rm $machine.tmp$$
  
  # strip genbank
  cat $machine.status$$ | egrep -v -f genbank.local$$ | grep -v gbDelete_tmp \
    > $machine.out$$
  rm -f $machine.status$$
end

# get diffs
commTrio.csh $mach1.out$$ $mach2.out$$ | sed -e "s/\.out$$//g" \
   | sed -e "s/Only/only/" > trioFile$$
echo
cat trioFile$$

# process the times or table names of tables that do not match
set firstOnly=`cat trioFile$$ | sed -n '1p' | awk '{print $1}'`
set secondOnly=`cat trioFile$$ | sed -n '2p' | awk '{print $1}'`
rm -f outFile$$
if ( 0 != $firstOnly || 0 !=  $secondOnly ) then
  cat $mach1.out$$.Only | sed -e "s/^/$mach1\t/" >> outFile$$
  cat $mach2.out$$.Only | sed -e "s/^/$mach2\t/" >> outFile$$
  sort -k2 outFile$$
  echo
endif

rm -f *out$$*
rm -f trioFile$$
rm -f outFile$$
rm -f genbank.local$$
