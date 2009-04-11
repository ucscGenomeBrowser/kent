#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  03-21-04
#  Checks grp table content on dev and beta, looking for changes
#  Prints diffs
#  Run this script on hgwdev
# 
#  29 April 2004 M. Chalup Added sort to dump files
###############################################

set db_new=""
set db_old=""
set table_a="grp"


if ($#argv != 2) then
  # no command line args
  echo
  echo "  Checks grp table content on dev and beta, looking for changes"
  echo "  prints diffs"
  echo
  echo "  usage:  command line args:  [dev_db_ver], [beta_db_ver]"
  echo
  exit
else
  set db_new=$argv[1]
  set db_old=$argv[2]
endif

echo "Compare content of hgwdev grp table with previous on hgwbeta (shows diffs):"
echo "using "$db_new" on hgwdev and "$db_old" on hgwbeta"

# --------------------------------------------
# compare description of grp table with previous (shows diffs):

echo
echo "                               hgwdev:hgwbeta"
echo "   Name                Label         Priority"
echo
hgsql -h $sqlbeta -N  -e "select * from $table_a" $db_old | sort  > $db_old.beta.$table_a.txt
hgsql -N  -e "select * from $table_a" $db_new | sort > $db_new.dev.$table_a.txt
diff $db_new.dev.$table_a.txt $db_old.beta.$table_a.txt
echo

