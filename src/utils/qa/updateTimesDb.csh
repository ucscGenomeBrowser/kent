#!/bin/tcsh


############################
# 
#  03-31-04
#  added command line db
#  added second db in case of proteinsYYMMDD updates
# 
#  will take one or two databases on command line
#  looks for second one on beta
#  if only one db listed, looks for it on dev and beta
#
#  will show if a table has been added or dropped
#
############################


set db=""
set dbBeta=""

if ($1 == "") then
  echo
  echo "  gets list of update times for entire database from dev."
  echo "  will compare to database on beta with different name, if needed."
  echo "  also shows tables that were on beta but on dev."
  echo
  echo "    usage: databaseDev, [databaseBeta]"
  echo
  exit
else
  set db=$1
  if ($2 == "") then
    # no second db
    echo "databaseDev set to $db"
    echo "no db provided for Beta. setting to: $db"
    set dbBeta=$db
  else
    set dbBeta=$2
    echo "dbBeta = $2"
  endif
endif


# these files of table names are used by other programs, 
# including proteins.csh:
hgsql -N -e "SHOW TABLES" $db | sort > $db.tables
hgsql -N -h hgwbeta -e "SHOW TABLES" $dbBeta | sort > $dbBeta.beta.tables
echo "getting update times: $db"
echo "getting update times: $dbBeta"
echo

foreach table (`cat $db.tables`)
  echo $table
  echo "============="
  set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
  if ($dbBeta == "") then
    echo "using same database name for beta as for dev: " $db
  else
    echo "using databases:  dev: $db  beta: $dbBeta"
  endif
  set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' \
      $dbBeta | awk '{print $13, $14}'`
  echo "."$dev
  echo "."$beta
  echo
end
echo


# --------------------------------------------
#  check tables that are on beta only:

comm -13 $db.tables $dbBeta.beta.tables > xxBetaOnlyxx
set betaOnly=`wc -l xxBetaOnlyxx | gawk '{print $1}'`
if ($betaOnly != 0) then
  echo
  echo "these tables were on the beta verson, but are not on dev:"
  echo
  foreach table (`cat xxBetaOnlyxx`)
    echo $table
    echo "============="
    set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
        | awk '{print $13, $14}'`
    if ($dbBeta == "") then
      echo "using same database name for beta as for dev: " $db
      set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' \
          $db | awk '{print $13, $14}'`
    else
      echo "using databases:  dev: $db  beta: $dbBeta"
      set beta=`hgsql -h hgwbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' \
          $dbBeta | awk '{print $13, $14}'`
    endif
    echo "."$dev
    echo "."$beta
    echo
  end
  echo
  echo database on dev : $db
  echo database on beta: $dbBeta
  echo
endif

rm -f xxBetaOnlyxx
