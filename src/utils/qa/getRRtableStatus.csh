#!/bin/tcsh
source `which qaConfig.csh`

################################
#  12-31-04
#  gets the status of any table from an RR database
#  using mark's genbank dumps.
#
#  Robert Kuhn
################################

set db=""
set machine="rr"
set table=""
set field=""
set dumpfile=""
set fieldval=0
set fields = ( 'Name' 'Engine' 'Version' 'Row_format' 'Rows' \
   'Avg_row_length' 'Data_length' 'Max_data_length' 'Index_length' \
   'Data_free' 'Auto_increment' 'Create_time' 'Update_time' \
   'Check_time' 'Create_options' 'Comment' )


if ( $#argv < 3 || $#argv > 4 ) then
  echo
  echo "  gets the status for any field of a table from a database."
  echo "  using mark's genbank dumps."
  echo "    warning:  not in real time.  uses overnight dump."
  echo
  echo "    usage: database table field [hgwdev | hgwbeta | rr]"
  echo "    fields available: Name, Engine, Version, Row_format, Rows, "
  echo "        Avg_row_length, Data_length, Max_data_length, Index_length, "
  echo "        Data_free, Auto_increment, Create_time, Update_time, "
  echo "        Check_time, Create_options, Comment"
  echo
  echo "    defaults to rr"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set field=$argv[3]
endif

if ( $#argv == 4 ) then
  set machine=$argv[4]
  echo $machine | egrep -q "hgwdev|hgwbeta|rr"
  if ( $status ) then
    echo
    echo " fourth parameter must be hgwdev, hgwbeta or rr"
    echo
    exit 1
  endif
endif

# get file for non-real-time queries (used for rr later)
set dumpfile=`getRRdumpfile.csh $db $machine`
if ( $status ) then
  echo
  echo "  database $db -- not found in status dumps"
  echo
  exit 1
endif

# check that table exists in dump of this database
cat $dumpfile | grep -w "^$table" > /dev/null
if ( $status ) then
  echo
  echo "  table $table -- not found in database $db"
  echo
  exit 1
endif

# set variable to index of field in STATUS dump
set i=0
while ( $i < `echo $#fields | awk '{print $1+1}'` )
  echo $fields[$i] | grep -w $field > /dev/null
  if ( $status ) then
    set i=`echo $i | awk '{print $1+1}'`
  else
    set fieldval=$i
    set i=$#fields
  endif
end

if ( $fieldval == 0 ) then
  echo
  echo "  $field -- no such field in TABLE STATUS output"
  echo
  exit 1
endif

# print the field for the desired table
cat $dumpfile | grep -w "^$table" | sed -e "s/\t/\n/g" | sed -n "${fieldval}p" 

set debug="false"
#set debug="true"
if ( $debug == "true" ) then
  echo
  echo "database = $db"
  echo "table    = $table"
  echo "field    = $field"
  echo "machine  = $machine"
  echo
  echo "machpath = $machpath"
  echo "dumpfile = $dumpfile"
  echo "fieldval = $fieldval"
  echo
  # exit
endif

exit
