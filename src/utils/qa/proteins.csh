#!/bin/tcsh
source `which qaConfig.csh`

############################
# 
#  03-31-04
#  runs standard checks on several databases
# 
############################


set db1=""
set db2=""
set db3=""

if ($#argv != 2) then
  # no command line args
  echo
  echo "  runs test suite on proteins databases. "
  echo "  compares new to previous databases for proteinsYYMMDD, "
  echo "    spYYMMDD, go."
  echo "  compares version on dev to that on beta."
  echo
  echo "    usage:  YYMMDD (new), YYMMDD (old)"
  echo
  exit
else
  set db=$argv[1]
endif


set db1="proteins$argv[1] proteins$argv[2]"
set db2="sp$argv[1] sp$argv[2]"
set db3="go go"


foreach db ("$db1" "$db2" "$db3")

  #  check update times vs. beta
  set dev=`echo $db | awk '{print $1}'`
  set beta=`echo $db | awk '{print $2}'`
  echo "dev = $dev   beta = $beta"
  updateTimesDb.csh $dev $beta
  echo

  # check for missing tables from beta (added tables are caught elsewhere):
  # gets table lists from updateTimes.csh output"
  echo "tables missing from dev that were on beta:"
  comm -13 $dev.tables $beta.beta.tables > saveBeta
  set missing=`wc -l saveBeta | awk '{print $1}'`

  if ( $missing == 0 ) then
    echo "   none"
  else
    cat saveBeta
  endif
  rm saveBeta
  echo 

  echo
  echo -------------------------------------------------
  echo "check rowcounts("$dev") against beta ("$beta"):"
  echo dev listed first:
  echo

  foreach table (`cat $dev.tables`)
    echo $table
    echo "============="
    hgsql -N -e "SELECT COUNT(*) FROM $table" $dev
    hgsql -N -h $sqlbeta -e "SELECT COUNT(*) FROM $table" $beta
    echo
  end
  echo


  # -------------------------------------------------
  # compare description of all tables with previous:
  
  echo 
  echo -------------------------------------------------
  echo "compare description of all tables with previous \
         blank if the same:"
  foreach table (`cat $dev.tables`)
    echo $table
    hgsql -h $sqlbeta -N  -e "DESCRIBE $table" $beta >  $beta.beta.$table.desc
    hgsql -N  -e "DESCRIBE $table" $dev > $dev.dev.$table.desc
    diff $beta.beta.$table.desc $dev.dev.$table.desc
    echo
  end
  echo
  
  
  # -------------------------------------------------
  # compare number of indices on dev vs beta,
  # and show indices of tables that do not match:

  echo
  echo " -------------------------------------------------"
  echo "compare number of indices on dev vs beta \
         blank if the same:\n"
  rm -f xxIndexx
  foreach table (`cat $dev.tables`)
    set indexNumDev=`hgsql -N -e "SHOW INDEX FROM $table" $dev \
       | wc -l | gawk '{print $1}'`
    set indexNumBeta=`hgsql -h $sqlbeta -N -e "SHOW INDEX FROM $table" $beta \
       | wc -l | gawk '{print $1}'`
    echo $table
    if ($indexNumDev != $indexNumBeta) then
      echo $table >> xxIndexx
      echo "  number of indices does not match:"
      echo "  dev  = $indexNumDev"
      echo "  beta = $indexNumBeta"
      echo
      echo "on dev, ${dev}:"
      hgsql -t -e "SHOW INDEX FROM $table" $dev
      echo
      echo "on beta, ${beta}:"
      hgsql -t -h $sqlbeta -e "SHOW INDEX FROM $table" $beta
      echo
    endif
    echo
  end
  
  echo  "-------------------------------------------------"
  echo  "         change database.  leaving $db "
  echo  "-------------------------------------------------"
  
  echo
end
  
hgsql -N -h $sqlbeta -e "SHOW TABLES" go > go.tables.push

  echo  "-------------------------------------------------"
  echo  "    list of go tables for pushing to beta is in file:  "
  echo  "       go.tables.push"
  echo  "-------------------------------------------------"
  

  echo
  echo  "-------------------------------------------------"
  echo  "    remember to request autodump when tables are on RR "
  echo  "-------------------------------------------------"
  

rm -f xxIndexx
rm -f *desc
dumpEmpty.csh .

