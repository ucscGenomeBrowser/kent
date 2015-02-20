#!/bin/bash
# quit if something within the script fails
set -beEu -o pipefail
source `which qaConfig.bash`

umask 002

################################
#
#  02-13-2015
#  Matthew Speir
#
#  qaAutoTrack.sh
#  Performs basic QA for auto-pushed tracks, which includes:
#  - Checks when data for track was last updated
#  - Coverage from featureBits -countGaps
#  - Percentage difference in coverage between now and the last time the script was run
#
################################

##### Variables #####
# Set by command-line options
db=""
tableName=""
bigBedMode=""
verboseMode=""

# Other variables
currDate=$(date --rfc-3339=date)
output="" # holds output message
logUrl="http://genecats.cse.ucsc.edu/qa/test-results/qaAutoTrack"
logDir="/usr/local/apache/htdocs-genecats/qa/test-results/qaAutoTrack"
currLogFile=""
prevLogFile=""
prevLogDate=""

# Variables for issue checking
maxChange=0.1000
issue=false
issueNote=""
tooOld=""
percentDiff=""

# Usage message as variable
usage="
Performs basic QA for auto-pushed tracks, which includes:
- Checks when data for track was last updated
- Coverage from featureBits -countGaps
- Percentage difference in coverage between now and the last time the script was run

Usage: $0 database tableName [bigBed] [verbose]

Notes:
	Use 'bigBed' for tracks supported by bigBed files.
	For OMIM, ISCA, or ClinVar tracks use omim, isca, or clinvar as the table name.
	Can only be run once for each database/track pair per day.
"
##### Functions #####

# Output function
function outputCovDiff {
	if [[ $prevLogFile != "" ]]
	then
		# get info needed for diff
		rawCount=$(echo $tblCov | awk '{print $1}')
		prevCov=$(egrep -A2 "^$tableName" $prevLogFile | grep "^Coverage New" | cut -d" " -f3-)
		rawCountPrev=$(echo $prevCov | awk '{print $1}')

		# Calculate diff between new and old coverage
		rawCountDiff=$(echo $(expr $rawCount - $rawCountPrev)|tr -d -)
		rawCountAvg=$(expr $rawCount / 2 + $rawCountPrev / 2)
		percentDiff=$(awk -v rcd=$rawCountDiff -v rca=$rawCountAvg 'BEGIN{print rcd / rca}')

		# Build output string
		output+="$tableName\nLast updated: $tblDate\nCoverage New: $tblCov\nCoverage Old: $prevCov\nCoverage Diff: $(awk -v pd=$percentDiff 'BEGIN{print pd * 100}')%\n\n"
	else
		output+="$tableName\nLast updated: $tblDate\nCoverage New: $tblCov\n\n"
	fi
}

# Function to raise errors
function raiseIssue {
	# Raises an error if it's been too long since last update
	if [ $(date -d "$tblDate" +%s) -le $(date -d "$tooOld" +%s) ]
	then
		issue=true
		issueNote+="$tableName has not been updated since $tblDate, see $logUrl/$db.$tableName.$currDate.txt for more details\n"
	fi

	# Raises error if coverage diff between versions is too large
	if [[ "$percentDiff" != "" ]] && [[ "$percentDiff" > "$maxChange" ]]
	then
		issue=true
		issueNote+="Large coverage diff for $tableName, see $logUrl/$db.$tableName.$currDate.txt for more details\n"
	fi
}

##### Parse command-line input #####

# print usage
if (( $# < 2 )) || (( $# > 4 ))
then
	echo -e "$usage"
	exit 1
# set required variables
else
	db="$1"
	tableName="$2"
fi

# Setting optional  arguments
if [ $# ==  3 ]
then
	bigBedMode=$3
	if [[ $bigBedMode != "bigBed" ]]
	then
		verboseMode=$3
		bigBedMode=""
		# error if 3rd isn't one of two optional args
		if [[ $bigBedMode == "" ]] && [[ $verboseMode != "verbose" ]]
		then
			echo -e "$usage"
			exit 1
		fi
  	fi
fi

if [ $# == 4 ]
then
	bigBedMode=$3
	verboseMode=$4
	# error if 4th arg isn't verbose
	if [[ $verboseMode != "verbose" ]]
	then
		echo -e "$usage"
		exit 1
	fi
fi

##### Main Program #####

# set currLogFile
currLogFile="$logDir"/"$db.$tableName.$currDate.txt"

# set info for prevLog
prevLogDate+=$(ls -Llt --time-style long-iso $logDir|grep -v total|egrep -m 1 -oh "$db\.$tableName\.[0-9]{4}-[0-9]{2}-[0-9]{2}"|sed -e "s/$db\.$tableName\.//g")
if [ -e $logDir/$db.$tableName.$prevLogDate.txt ]
then
	prevLogFile="$logDir"/"$db.$tableName.$prevLogDate.txt"
fi

# Can't run twice in one day as it messes up the "Coverage Old" output
if [[ $currDate == $prevLogDate ]]
then
	echo -e "Previous log date is the same as today's date, $currDate"
	exit 1
fi

# Set tooOld for different tables
if  [[ $tableName == clinvar ]] || [[ $tableName == grcIndcidentDb ]]
then 
	tooOld=$(date -d "$currDate - 1 month" +%F)
else
	tooOld=$(date -d "$currDate - 15 days" +%F)
fi

# Run tests for different tracks 
if [[ $bigBedMode == "bigBed" ]]
then
	# ClinVar has muliple tables
	if [[ $tableName == "clinvar" ]]
	then
		for tbl in clinvarMain clinvarCnv
		do
			# Get file name from beta
			fileName=$(hgsql -h mysqlbeta -Ne "SELECT * FROM $tbl" $db)
			# Get table update time from beta
			tblDate=$(ssh qateam@hgwbeta "date -d '$(stat -Lc '%y' $fileName)' +%F' '%T")
			# featureBits doesn't work with bigBeds, need to turn into bed first
			ssh qateam@hgwbeta "/usr/local/apache/cgi-bin/utils/bigBedToBed $fileName stdout" > $TMPDIR/temp.$tbl.bed
			tblCov=$(featureBits -countGaps $db $TMPDIR/temp.$tbl.bed 2>&1)
			# outoutCovDiff function needs variable tableName
			tableName=$tbl

			outputCovDiff

			# Check for issues with table
			raiseIssue

			rm -f $TMPDIR/temp.$tbl.bed
		done
	# GRC Incident track relies on remote file so curl must be used instead of stat
	elif [[ $tableName == "grcIncidentDb" ]]
	then
		fileName=$(hgsql -h mysqlbeta -Ne "SELECT * FROM $tableName" $db)
		# Use curl to get update time on file
		tblDate=$(date -d "$(curl -s -v -X HEAD $fileName 2>&1 | grep '^< Last-Modified:'| cut -d" " -f3- )" +%F" "%T)
		# featureBits doesn't work with bigBeds, need to turn into bed first
		bigBedToBed $fileName $TMPDIR/temp.$tableName.bed
		tblCov=$(featureBits -countGaps $db $TMPDIR/temp.$tableName.bed 2>&1)

		outputCovDiff

		# Check for issues with table
		raiseIssue
			
		rm -f $TMPDIR/temp.$tableName.bed
	# Tests for all other bigBed based autopushed tracks (assuming they don't use remote bigBed files)
	else
		fileName=$(hgsql -h mysqlbeta -Ne "SELECT * FROM $tableName" $db)
		# Get table update time from beta
		tblDate=$(ssh qateam@hgwbeta "date -d '$(stat -Lc '%y' $fileName)' +%F' '%T")
		# featureBits doesn't work with bigBeds, need to turn into bed first
		ssh qateam@hgwbeta "/usr/local/apache/cgi-bin/utils/bigBedToBed $fileName stdout" > $TMPDIR/temp.$tbl.bed
		tblCov=$(featureBits -countGaps $db $TMPDIR/temp.$tbl.bed 2>&1)

		outputCovDiff
	
		# Check for issues with table
		raiseIssue

		rm -f $TMPDIR/temp.$tableName.bed
	fi
# Tests for non-bigBed tracks
else
	# OMIM and ISCA both have a large number of tables
	if [[ $tableName == "omim" ]] || [[ $tableName == "isca" ]]
	then
		for tbl in $(hgsql -h mysqlbeta -Ne "SHOW TABLES LIKE '%$tableName%'" $db) # Grabs list of all omim or isca tables from beta
        	do
	               	tblDate=$(hgsql -h mysqlbeta -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$db' AND TABLE_NAME='$tbl'")
			# Only some omim tables have coordinates
                	if [[ $tbl == "omimGene2" ]] || [[ $tbl == "omimAvSnp" ]] || [[ $tbl == "omimLocation" ]] || [[ $tableName == "isca" ]]
                	then
                        	tblCov=$(ssh qateam@hgwbeta "featureBits -countGaps $db $tbl 2>&1")
				# temporary holder so we don't loose original input tableName
				tableNameTemp=$tableName
				# set tableName to tbl temporarily so we can use one output function for all tables
				tableName=$tbl
				
				outputCovDiff
				# reset tableName to original name
				tableName=$tableNameTemp
			# Output for tables that don't contain coordinates
			else
				output+="$tbl\nLast updated: $tblDate\n\n"
			fi
		done
		# Check for different issues with table
		# Must be outside of for loop so as to only output one error message for the entire table set	
		raiseIssue
	# Tests for all other table based autopushed tracks
	else
		tblDate=$(hgsql -h mysqlbeta -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$db' AND TABLE_NAME='$tableName'")
		tblCov=$(ssh qateam@hgwbeta "featureBits -countGaps $db $tableName 2>&1")

		outputCovDiff

		# Check for issues with table
		raiseIssue
	fi
fi

# Output results of tests
if [[ $issue == true ]]
then
	if [[ $verboseMode != "" ]] # True if verboseMode is on
	then
		#print error message
		echo -e "$issueNote" | tee $currLogFile
		#print output to log file and to screen
		echo -e $output | tee -a $currLogFile
	else
		#print error message	
		echo -e "$issueNote" | tee $currLogFile
		#print output to log file
		echo -e $output >> $currLogFile
	fi
else
	if [[ $verboseMode != "" ]] # True if verboseMode is on
	then
		#print output to log file and to screen
		echo -e $output | tee $currLogFile
	else
		#print output to log file
		echo -e $output > $currLogFile
	fi
fi
