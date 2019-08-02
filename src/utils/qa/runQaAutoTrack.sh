#!/bin/bash
# quit if something within the script fails
set -beEu -o pipefail
source `which qaConfig.bash`
umask 002

################################
#
#  01-13-2017
#  Matthew Speir
#
#  runQaAutoTrack.sh
#  Runs script bin/qaAutoTrack.sh for 
#  various autopushed tracks
#
#  Runs day after track is supposed
#  to be updated for most tracks
#
################################

# Set current day of the week
# Needed to determine which tracks to test
dayOfWeek=$(date +%A)
# Set date used to delete logs older than this. 
# Currently set to 6mo ago
logTooOld=$(date --date="-6 months" +%F)
# Where qaAutoTrack is placing the results
logDir=""

usage="""
Runs script qaAutoTrack.sh for various autopushed tracks.\nWill
remove log files older than six months from log directory\nfor
qaAutoTrack.sh. Log directory defaults to\n
/usr/local/apache/htdocs-genecats/qa/test-results/qaAutoTrackLogs
\n\n
usage:`basename $0` go logDirectory
\n\nlogDirectory is directory where output files are being placed\nby
qaAutoTrack.sh if it's different that the default listed above.\n
"""
# Print really basic usage statement
if (( $# < 1 )) || (( $# > 2 ))
then
	echo -e $usage
	exit 1
elif (( $# == 1 ))
then
        runScript=$1
elif (( $# == 2 ))
then
	# Check if logDirectory exists
	if [ -d $2 ]
	then
        	runScript="$1"
		logDir="$2"
	else
		echo -e "Sorry, directory \"$2\" does not exist. "
		echo -e "Check spelling or create this directory and try again.\n"
		exit 1
	fi
fi

if [[ $logDir == "" ]]
then
	logDir="/usr/local/apache/htdocs-genecats/qa/test-results/qaAutoTrackLogs"
fi

# Print usage statement if the supplied argument isn't "go"
if [[ $runScript != "go" ]]
then
	echo -e $usage
	exit 1
# Run qaAutoTrack.sh for different tracks depending on the day of the week
else
	if [ -e  "$logDir/*.txt" ]
	then
		# Identify log files that are greater than 6 months old.
		# $1, $2, $3, and $4 are the db, table, date, and time respectively.
		# $5 is the file extension (txt).
		oldLogFiles=$(ls $logDir/*.txt | awk -v ymdOld=$logTooOld -F'.' '$3 <= ymdOld {print $1"."$2"."$3"."$4"."$5}')
		# Remove these old log files
		# Prevents log file directory from becoming overstuffed with files
		for file in $(echo $oldLogFiles)
		do
			rm $file
		done
	fi

	if [[ $dayOfWeek == "Monday" ]]
	then
		qaAutoTrack.sh hg19 isca 
		qaAutoTrack.sh hg38 isca 
		qaAutoTrack.sh hg19 decipher 
		qaAutoTrack.sh hg19 geneReviews 
		qaAutoTrack.sh hg38 geneReviews 
	elif [[ $dayOfWeek == "Tuesday" ]]
	then
		qaAutoTrack.sh hg18 lovd 
		qaAutoTrack.sh hg19 gwasCatalog 
		qaAutoTrack.sh hg38 gwasCatalog 
	elif [[ $dayOfWeek == "Wednesday" ]]
	then
		qaAutoTrack.sh -b danRer10 grcIncidentDb
		qaAutoTrack.sh -b galGal5 grcIncidentDb
		qaAutoTrack.sh -b hg19 grcIncidentDb
		qaAutoTrack.sh -b hg38 grcIncidentDb
		qaAutoTrack.sh -b mm9 grcIncidentDb
		qaAutoTrack.sh -b mm10 grcIncidentDb
	elif [[ $dayOfWeek == "Thursday" ]]
	then
		qaAutoTrack.sh hg19 omim 
		qaAutoTrack.sh hg38 omim 
		qaAutoTrack.sh -b hg19 clinvar
		qaAutoTrack.sh -b hg38 clinvar
	elif [[ $dayOfWeek == "Friday" ]]
	then
		qaAutoTrack.sh hg38 refGene
	fi
fi
