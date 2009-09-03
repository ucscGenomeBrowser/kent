#!/bin/tcsh
source `which qaConfig.csh`


#################################
# 
# Bob Kuhn -- 2009 
# 
#  This script gets the date from a table 
#  and converts it to a touch command
#  for moving tables from MySQL 4.0 to 5.0 
#  and preserving timestamp
# 
#################################

set host=""
set db=""
set table=""
set tables=""
set files=""

if ( $#argv != 3 ) then
  echo
  echo "  gets the Update_time from a table and converts to a touch command."
  echo
  echo "    usage:  machine database table(list) "
  echo
  echo "     will accept single or list of tables."  
  echo
  exit
else
  set host=$argv[1]
  set db=$argv[2]
  set table=$argv[3]
endif


# check to see if it is a single fileName or a filelist
file $table | egrep -q "ASCII"
if ( ! $status ) then
  set tables=`cat $table`
else
  set tables=$table
endif

set ver=` hgsql -h $host -Ne "show variables" hg18 | grep version \
  | grep log | awk '{print $2}' | awk -F. '{print $1}'`
if ( $status ) then
  exit 1
endif
set subver=` hgsql -h $host -Ne "show variables" hg18 | grep version \
  | grep log | awk '{print $2}' | awk -F. '{print $2}'`

echo
foreach table ( $tables )
  if ( 4 == $ver && 1 == $subver || 5 == $ver ) then
      # newer mysql versions use different fields
    set date=`hgsql -h $host -Ne 'SHOW TABLE STATUS LIKE "'$table'"' $db \
     | awk '{print $14, $15}'`
  else
    set date=`hgsql -h $host -Ne 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  endif
  set timestamp=`echo $date | sed -r "s/(:..):(..)/\1\.\2/" | sed "s/[- :]//g"`
  echo "touch -m -t $timestamp /var/lib/mysql/$db/$table.*"
end
echo


