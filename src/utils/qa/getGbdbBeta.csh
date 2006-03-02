#!/bin/tcsh

####################
#  06-20-05 Robert Kuhn
#
#  Script to dump /gbdb files from beta
#
####################

set filePath=""
set db=""
set todayDate=`date +%Y%m%d`

if ($#argv != 1) then
  echo
  echo "  script to dump most /gbdb files from beta."
  echo "    gets all assembly files except ci1 and zoo."
  echo "    uses list of assemblies on beta."
  echo "       also list hgFixed."
  echo "    ignores files with Scaffold in the name "
  echo "       unless a db that uses them is specified."
  echo "    ignores: axtNetDp1, ci1, zoo, /sacCer1/sam."
  echo
  echo '      usage:  database or path (will use "all")'
  echo
  exit
else
  set filePath=$argv[1]
  set outpath=`echo $filePath | sed -e 's/\//./'`
  set      db=`echo $filePath | sed -e 's/\/.*//'`
endif

set output=$outpath.gbdb.out
set output2=$outpath.gbdb.list
  # echo "filePath = $filePath"
  # echo "outpath  = $outpath"
  # echo "db       = $db"
  # echo "output   = $output"
  # echo "output2  = $output2"

rm -f $output
rm -f $output2

# get assembly list from beta
hgsql -h hgwbeta -N -e "SELECT name FROM dbDb" hgcentralbeta \
   | sort | grep -v ci1 | grep -v zoo > xxAssembliesxx
echo "all" >> xxAssembliesxx
echo "genbank" >> xxAssembliesxx
echo "hgFixed" >> xxAssembliesxx

# error checking for db name on command line
# flag set to 1 if $db is actually an assembly (or genbank, all or hgFixed)"
set dbFlag=0
foreach assembly (`cat xxAssembliesxx`)
  # echo $assembly
  if ($db == "$assembly") then
    set dbFlag=1
    break
  endif
end

if ($dbFlag == "0") then
  echo '\n  '$db' is not an active assembly. try another assembly name, or "all"'
  echo
  exit
endif

# get the files from beta
rm -f xxfullListxx
if ($db == "all") then
  foreach assembly (`cat xxAssembliesxx | grep -v "all"`)
    # do not check Kevin's voluminous /sacCer1/sam/* files:
    if ( $assembly == "sacCer1" ) then
      set yeDirs=`ssh hgwbeta ls /gbdb/$assembly | grep -wv sam`
      foreach yedir ( $yeDirs )
        ssh hgwbeta find /gbdb/$assembly/$yedir -type f -print \
          | grep -vi Scaffold \
          | grep -v axtNetDp1   | grep -v "sacCer1/sam/" > tempfile
          # grep -v description | grep -v genbank \
        cat tempfile >> xxfullListxx
        cat tempfile
        rm -f tempfile
      end
    else 
      ssh hgwbeta find /gbdb/$assembly -type f -print | grep -vi Scaffold \
         | grep -v axtNetDp1   | grep -v "sacCer1/sam/" > tempfile
         # grep -v description | grep -v genbank \
      cat tempfile >> xxfullListxx
      cat tempfile
      rm -f tempfile
    endif
  end
else
  if (-e /gbdb/$filePath) then
    ssh hgwbeta find /gbdb/$filePath -type f -print \
         | grep -v axtNetDp1   | grep -v "sacCer1/sam/" \
         | grep -v description > xxfullListxx
  else
    echo "\n  directory /gbdb/$filePath does not exist\n"
    exit
  endif
endif

sort xxfullListxx > gbdb.$db.$todayDate
cp gbdb.$db.$todayDate /usr/local/apache/htdocs/qa/test-results/gbdb

# clean up
rm -f xxAssembliesxx
rm -f xxfullListxx
