#!/bin/tcsh

####################
#  07-09-04 Bob Kuhn
#
#  Script to check for /gbdb sync
#
####################

set db="all"
set errFlag=0
set debug="true"
set debug="false"

if ($#argv != 1) then
  echo
  echo "  script to check for /gbdb sync."
  echo "    checks only active assemblies (but not ci1), "
  echo "      unless other is specified."
  echo "    ignores scaffolds unless a db that uses them is specified."
  echo "    ignores: description genbank axtNetDp1 ci1 zoo."
  echo
  echo '      usage:  database (will use "all")'
  echo
  exit
else
  set db=$argv[1]
endif

set output=$db.gbdb.out
set output2=$db.gbdb.list

rm -f $output
rm -f $output2

# get assembly list from beta
hgsql -h hgwbeta -N -e "SELECT name FROM dbDb" hgcentralbeta \
   | sort | grep -v ci1 | grep -v zoo  > xxAssembliesxx
echo "all" >> xxAssembliesxx

# error checking for db name on command line
set dbFlag=0
foreach assembly (`cat xxAssembliesxx`)
  # echo $assembly
  if ($db == "$assembly") then
    set dbFlag=1
    break
  endif
end

if ($dbFlag == "0") then
  echo '\n'$db' is not an active assembly. try another assembly name, or "all"'
  echo
  exit
endif

# get the files from beta
rm -f xxDirlistxx
if ($db == "all") then
  foreach assembly (`cat xxAssembliesxx | grep -v "all"`)
    ssh hgwbeta find /gbdb/$assembly -type f -print | grep -vi Scaffold \
         | grep -v description | grep -v genbank \
         | grep -v axtNetDp1   >> xxDirlistxx
  end
  echo "no assemblies with scaffolds are checked\n" >> $output
else
  if (-e /gbdb/$db) then
    ssh hgwbeta find /gbdb/$db -type f -print \
         | grep -v description > xxDirlistxx
  else
    echo "\ndirectory /gbdb/$db does not exist\n"
    exit
  endif
endif


# compare the files from beta on dev
set errFlag=0
foreach file (`cat xxDirlistxx`)
  # echo $file
  if (-e $file) then
    set dev=`ls -ogL $file`
  else
    set dev=""
  endif
  set beta=`ssh hgwbeta ls -og $file`
  if ("$dev" != "$beta") then
    echo $dev >> $output2
    set errFlag=1
    echo ". $dev" >> $output
    echo ". $beta" >> $output
    echo "-------------------------" >> $output
  endif

  if ($debug == "true") then
    echo $file
    echo "dev  = $dev"
    echo "beta = $beta"
    echo "-------------------------"
    echo
  endif
end

# output only if problem is found
if ($errFlag == "1") then
  cat $output 
else
  echo "\nall files match on dev and hgnfs1\n"
  rm -f $output
endif

# clean up
rm -f xxDirlistxx
rm -f xxAssembliesxx
