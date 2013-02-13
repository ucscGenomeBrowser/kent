#!/bin/tcsh
source `which qaConfig.csh`

###############################################
# 
#  04-09-07
#  Robert Kuhn
#
#  Grab hgcentral* tables from dev, beta, rr and store as files.
# 
###############################################

set directory="" 
set today=""
set dirPath=""
set urlPath=""
set lastYr=""
set devString=""
set betaString=""
set rrString=""
 
if ($#argv == 0 || $#argv > 1) then
  # no command line args
  echo
  echo "  grab hgcentral* tables from dev, beta, rr and store as files."
  echo
  echo "    usage:  go"
  echo
  exit
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set directory="hgcentral" 
# set file paths and URLs
# set today="2008-05-05"
set today=`date +%Y-%m-%d`
set dirPath="/usr/local/apache/htdocs-genecats/qa/test-results/$directory"
rm -rf $dirPath/$today/
mkdir -p $dirPath/$today
set urlPath="http://genecats.cse.ucsc.edu/qa/test-results/$directory/$today"

# remove last year's dir
set lastYr=`date +%Y-%m --date='1 year  ago'`
rm -r  $dirPath/${lastYr}*

set devString="hgcentraltest"
set betaString="-h $sqlbeta hgcentralbeta"
set rrString="-h $sqlrr hgcentral" 

foreach table ( blatServers clade dbDb dbDbArch defaultDb genomeClade \
  liftOverChain namedSessionDb targetDb )
  hgsql  $devString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/hgwdev.$table
  hgsql $betaString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/hgwbeta.$table
  hgsql   $rrString -N -e "SELECT * FROM $table" | sort >> $dirPath/$today/rr.$table
end

echo $urlPath
chmod 650 $dirPath/$today/*.namedSessionDb
