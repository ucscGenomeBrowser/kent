#!/bin/tcsh
source `which qaConfig.csh`

###########################################
# 
# This script checks the stats for the RR machines
# and creates a report on user's browsers and o/s's
# This report is placed here: hgwdev:/qa/test-results/browserUsage
# This script is run monthly by a qateam cron job.
#
############################################

set fileLocation="/usr/local/apache/htdocs-genecats/qa/test-results/browserUsage"
set fileName='monthly.report.html'
set lineNum=0
set shortFile=''
set shorterFile=''
set shortestFile=''
set today=`date +%Y-%m-%d`

# usage statement
if ( $argv != 'go' ) then
  echo
  echo " Checks stats for browser and o/s usage."
  echo " Places report here:\
         http://genecats.soe.ucsc.edu/qa/test-results/browserUsage"
  echo "  usage: go"
  echo
  exit
endif 

# run this only on hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\n ERROR: you must run this script on dev!\n"
  exit 1
endif

# get a copy of the statistics file from the hgw1 server
scp -q qateam@hgw1:/usr/local/apache/htdocs/admin/stats/Report.html $fileName

# stop if you can't find the Report
if ( $status ) then
  exit 1
endif

# delete the parts we do not need to display
# delete everything from the start of the file to (but not including) <div class="browsum">
set lineNum=`cat $fileName | grep -n '<div class="browsum">'`

# get just the number part of the line and subtract one from it
set lineNum=`echo $lineNum | sed 's/:.*$//' | gawk '{print $1 -1}'`

# delete lines 1 through $lineNum
cat $fileName | sed "1,$lineNum d" > shortFile

# delete everything from (and including) <div class="code"> to the end
set lineNum=`cat shortFile | grep -n '<div class="code">'`

# get just the number part of the line
set lineNum=`echo $lineNum | sed 's/:.*$//'`

# delete lines $lineNum through end
cat shortFile | sed "$lineNum,$ d" > shorterFile

# prep the file
echo "<HTML><BODY>" > shortestFile
echo 'See all the raw <A HREF="http://genome.soe.ucsc.edu/admin/stats/" TARGET=_blank>statistics</A>' >> shortestFile

# remove sub-items from the lists
cat shorterFile | grep -v 'goto' | grep -v 'level2' >> shortestFile

# append to the file
echo "</BODY></HTML>" >> shortestFile

# make the file viewable
chmod 775 shortestFile

# move the file to the test-results location
mv shortestFile /usr/local/apache/htdocs-genecats/qa/test-results/browserUsage/$today.html

# remove all the intermediate files
rm shortFile
rm shorterFile
rm $fileName
