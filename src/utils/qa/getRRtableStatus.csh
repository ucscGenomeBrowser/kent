#!/bin/tcsh

################################
#  12-31-04
#  gets the status of any table from an RR (or mgc) database
#  using mark's genbank dumps.
#
#  Robert Kuhn
################################

set database=""
set machine="hgw1"
set table=""
set field=""
set dumpfile=""

if ( $#argv < 3 | $#argv > 4 ) then
  echo
  echo "  gets the status of any table from an RR (or mgc) database."
  echo "  using mark's genbank dumps."
  echo "    warning:  not in real time.  uses overnight dump."
  echo
  echo "    usage: database, table, field, [RRmachine] (defaults to hgw1)"
  echo "      fields available: Name, Type, Row_format, Rows, Avg_row_length, "
  echo "         Data_length, Max_data_length, Index_length, Data_free, "
  echo "         Auto_increment, Create_time, Update_time, Check_time, "
  echo "         Create_options, Comment"
  echo
  exit
else
  set database=$argv[1]
  set table=$argv[2]
  set field=$argv[3]
endif

if ( $#argv == 4 ) then
  set machine=$argv[4]
endif

set dumpfile=`getRRdumpfile.csh $database $machine`

# check that table exists in dump of this database
cat $dumpfile | grep -w "^$table" > /dev/null
if ( $status ) then
  echo
  echo "  table $table -- not found in database $database"
  echo
  exit 1
endif

# check if $field is legit
head -1 $dumpfile | grep -iw "$field" > /dev/null
if ( $status ) then
  echo
  echo "  $field -- no such field in TABLE STATUS output"
  echo "  for $database.$table.  try one of the following"
  echo
  head -1 $dumpfile | sed -e "s/\t/\n/g" 
  echo
  exit 1
endif


# set variable to index of field in STATUS dump
set fieldval=`head -1 $dumpfile | sed -e "s/\t/\n/g" | grep -iwn "$field" \
      | gawk -F":" '{print $1}'`

# print the field for the desired table
cat $dumpfile | grep -w "^$table" | sed -e "s/\t/\n/g" | sed -n "${fieldval}p" 

set debug="false"
# set debug="true"
if ( $debug == "true" ) then
  echo
  echo "database = $database"
  echo "table    = $table"
  echo "field    = $field"
  echo "machine  = $machine"
  echo
  echo "machpath = $machpath"
  echo "fullpath = $fullpath"
  echo "dumpfile = $dumpfile"
  echo "fieldval = $fieldval"
  echo
  # exit
endif

exit 1

# rm -f tablelist
