#!/bin/tcsh
source `which qaConfig.csh`

################################
#  05-17-04
#  checks all tables on beta and looks to see if they have been updated on dev.
#  uses /cluster/data/genbank/etc/genbank.tbls
#
################################

set databases=""
set ignore="hgFindSpec gbDelete_tmp tableDescriptions extFile seq"
set exclude=""

if ($#argv != 1) then
  echo
  echo "  checks all tables on beta to see if they have been updated on dev."
  echo "  uses /cluster/data/genbank/etc/genbank.tbls to ignore genbank."
  echo '  will exclude any db.table listed in local file called "excludeTables".'
  echo '  excludes split tables of the form "chrN_".'
  echo
  echo "    usage:  <database | assemblies | all>"
  echo '       "assemblies" checks only databases in dbDb (all organisms, no others).'
  echo
  exit
else
  set databases=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

echo $databases > dbs

if ($databases == assemblies) then
  hgsql -N -h $sqlbeta -e "SELECT name FROM dbDb" hgcentralbeta > dbs
endif

if ($databases == all) then
  hgsql -N -h $sqlbeta -e "SHOW DATABASES" hgcentralbeta > dbs
endif

echo
echo "these tables from beta do not match on dev:"
echo "update times (dev first):"
echo

foreach db (`cat dbs`)

  if (-e excludeTables) then
    # split the list into chrN-style and the others
    # improve this using getSplit.csh
    cat excludeTables | grep $db |  awk -F. '{print $2}' > $db.exclude
    cat $db.exclude | grep "chr*_*" | sed -e "s/^.*_//" > $db.chrN.exclude
    cat $db.exclude | grep -v "chr*_*" | sed -e "s/^.*_//" > $db.notChrN.exclude
  endif
  # echo "exclude = $exclude"
  # echo "cat the file:"
  # cat $db.exclude
  # cat $db.chrN.exclude

  rm -f $db.strip
  # strip out genbank chrN tables plus any chrN table in excludeTables
  hgsql -N -e "SELECT chrom FROM chromInfo" $db > $db.chroms

  foreach chrom (`cat $db.chroms`)
    echo ${chrom}_est >> $db.strip
    echo ${chrom}_intronEst >> $db.strip
    echo ${chrom}_mrna >> $db.strip
    if (-e $db.chrN.exclude) then
      foreach chrN (`cat $db.chrN.exclude`)
        echo ${chrom}_$chrN >> $db.strip
      end
    endif
  end
  # echo
  # echo "cat the strip file:"
  # cat $db.strip

  # get list of genbank tables to remove from list (and remove ^ and $)
  cat /cluster/data/genbank/etc/genbank.tbls | sed -e 's/^^//; s/.$//' \
    > genbank.local

  # add genbank list to chrN_strip list and sort
  cat $db.strip genbank.local | sort > $db.genbankPlus

  # get list of tables from beta, remove trackDb* 
  hgsql -N -h $sqlbeta -e "SHOW TABLES" $db | sort | grep -v "trackDb" \
    > $db.tables.beta

  rm -f $db.remove
  foreach table ($ignore `cat $db.notChrN.exclude`)
    echo $table >> $db.remove
  end
  cat $db.remove >> $db.genbankPlus
  sort $db.genbankPlus > $db.remove.full

  comm -23 $db.tables.beta $db.remove.full > $db.tables 

  # clean up
  rm -f $db.tables.beta
  rm -f $db.strip*
  rm -f $db.chroms
  rm -f $db.genbankPlus
  rm -f $db.exclude
  rm -f $db.chrN.exclude
  rm -f $db.notChrN.exclude
  rm -f $db.remove
  rm -f $db.remove.full

  echo
  echo "============= ============= ============="
  echo "database = $db"
  echo
  
  rm -f $db.outOfSync
  foreach table (`cat $db.tables`)
    set dev=`hgsql -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`
    set beta=`hgsql -h $sqlbeta -N -e 'SHOW TABLE STATUS LIKE "'$table'"' $db \
      | awk '{print $13, $14}'`

    if ("$beta" != "$dev") then
      echo $table >> $db.outOfSync
      echo $table
      echo "============="
      echo "."$dev
      echo "."$beta
      echo
    endif
  end
  echo
  
end

exit
  
