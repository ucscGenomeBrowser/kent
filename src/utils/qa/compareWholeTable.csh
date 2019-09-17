#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  07-14-04
#  Robert Kuhn
# 
#  Gets an entire table from dev and beta and check diffs
# 
###############################################

set db=""
set table=""

set mach1="hgwdev"
set mach2="hgwbeta"
set host1=""
set host2=""
set rr1="false"
set rr2="false"

set first=""
set second=""

if ($#argv != 2 && $#argv != 4) then
  echo
  echo "  gets an entire table from two machines and checks diffs."
  echo "  reports numbers of rows unique to each and common."
  echo "  writes files of everything."
  echo "  not real-time on RR -- uses genome-mysql."
  echo
  echo "    usage:  database table [machine1] [machine2]"
  echo "      (defaults to dev and beta)"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

# --------------------------------------------

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv == 4) then
  set mach1=$argv[3]
  set mach2=$argv[4]
endif

# confirm that machine names are legit
checkMachineName.csh $mach1 $mach2
if ( $status ) then
  exit 1
endif

if ( $mach1 == "hgwbeta" ) then
  set host1="-h $sqlbeta"
endif

if ( $mach2 == "hgwbeta" ) then
  set host2="-h $sqlbeta"
endif
 
# set flags for RR queries
if ( $mach1 != "hgwdev" &&  $mach1 != "hgwbeta" ) then
  set rr1="true"
endif

if ( $mach2 != "hgwdev" &&  $mach2 != "hgwbeta" ) then
  set rr2="true"
endif

set outfile1=$db.$table.$mach1
set outfile2=$db.$table.$mach2
if ( $rr1 == "true" ) then
  nice mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
    -N -e "SELECT * FROM $table" $db | sort > $outfile1
else
  nice hgsql $host1 -N -e "SELECT * FROM $table" $db | sort > $outfile1
endif

if ( $rr2 == "true" ) then
  nice mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
    -N -e "SELECT * FROM $table" $db | sort > $outfile2
else
  nice hgsql $host2 -N -e "SELECT * FROM $table" $db | sort > $outfile2
endif

commTrio.csh $outfile1 $outfile2 | grep -v "output files" \
  | sed -e "s/$outfile1.$outfile2.Only/$outfile1.$mach2.Only/"
mv $outfile1.$outfile2.Only $outfile1.$mach2.Only

