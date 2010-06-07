#!/bin/tcsh
source `which qaConfig.csh`

################################
#  05-19-04
#  gets the rowcount for a list of tables.
#
################################

set db=""
set tablelist=""
set tables=""
set dev=""
set beta=""
set rr=""
set doGenomeMysql=false
set genomeMysql=""

if ($#argv < 2 || $#argv > 3) then
  echo
  echo "  gets the rowcount for a list of tables from dev, beta and RR."
  echo
  echo "    usage:  database tablelist [genome-mysql]"
  echo "      tablelist can be just name of single table"
  echo
  echo "    RR results not in real time, but from dumps"
    echo "    genome-mysql option adds results from public mysql server"
  echo
  exit
else
  set db=$argv[1]
  set tablelist=$argv[2]     # file of tablenames or single table name
endif

if ( $#argv == 3 ) then
  if ( $argv[3] == "genome-mysql" ) then
    set doGenomeMysql=true
  else
    echo
    echo 'error: third argument must be "genome-mysql"' > /dev/stderr
    $0
    echo
    exit 1
  endif
endif

echo
if ( -e $tablelist ) then
  echo "running countRows for tables:"
  set tables=`cat $tablelist`
  echo "$tables"
  echo
else
  set tables=$tablelist
endif

foreach table ( $tables )
  set dev=`hgsql -N -e "SELECT COUNT(*) FROM $table" $db` 
  set beta=`hgsql -h $sqlbeta -N -e "SELECT COUNT(*) FROM $table" $db`
  set rr=` getRRtableStatus.csh $db $table Rows`
  if ( $doGenomeMysql == true ) then
    set genomeMysql=`mysql -N -h genome-mysql -u genome -A -e "SELECT COUNT(*) FROM $table" $db`
  endif
  echo $table
  echo "============="
  echo "."$dev 
  echo "."$beta 
  echo
  echo "."$rr
  if ( $doGenomeMysql == true ) then
    echo
    echo "."$genomeMysql
    echo
  endif
  echo
end

