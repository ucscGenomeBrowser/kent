#!/bin/bash
# quit if something within the script fails
set -beEu -o pipefail
source `which qaConfig.bash`
export HGDB_CONF=$HOME/.hg.conf.beta
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
verboseMode=false
overwriteLogUrl=false
newLogDir=""

# Other variables
currDate=$(date +%F)
currTime=$(date +%H_%M_%S)
output="" # holds output message
logUrl="http://genecats.soe.ucsc.edu/qa/test-results/qaAutoTrackLogs"
logDir="/usr/local/apache/htdocs-genecats/qa/test-results/qaAutoTrackLogs"
currLogFile=""
prevLogFile=""
prevLogDate=""

# Variables for issue checking
issueNote=""
tooOld=""
percentDiff=""

##### Functions #####

# Usage message function
showHelp() {
cat << EOF
Usage: `basename $0` [-hbvs] [-l log dir] [-u log url] \$database \$table

	Required arguments:
	database	 UCSC database name, e.g. hg19 or hg38.
	table		 Table name, e.g. gwasCatalog.

	Optional arguments:
	-h		 Display this help and exit
	-b		 BigBed mode. Used for tracks supported
			 bigBed files, i.e. grcIncidentDb.
	-v		 Verbose mode. Outputs test results to
			 standard out as well as file.
	-l log directory Alternate directory for output log
	-u log url	 Alternate URL for output log.
	-s		 Suppress URL in output. Will instead
			 print directory in messages.

Performs basic QA for auto-pushed tracks, which includes:
- Checks when data for track was last updated
- Coverage from featureBits -countGaps
- Percentage difference in coverage between now and
  the last time the script was run

By default, all results are output to a file and only issues
are output to the command line. Use the "-v" option to see
results on the command line as well. All log files are output to:
http://genecats.soe.ucsc.edu/qa/test-results/qaAutoTrack.

Notes:
	- For OMIM, ClinGen (formerly ISCA), or ClinVar tracks use omim, isca, or clinvar as the table name.
	- If run more than once per day, then subsequent log files will include current time in name.

EOF
}

# Output function
function outputCovDiff () {
	# four positional arguments. $1 == prevLogFile. $2 == tblCov. $3 == tableName. $4 == tblDate.
	# Dependency: Coverage information from previous log file (positional argument $1)
	# If no previous log file exists or if previous log file doesn't contain coverage info,
	# then no coverage diff will be calculated and the percentDiff variable will retain its
	# default value of ""

	if [[ $1 != "" ]] # Check for previous log file. True if file exists.
	then
		# get info needed for diff
		rawCount=$(echo $2 | awk '{print $1}')
		prevCov=$(egrep -A2 "^$3" $1 | grep "^Coverage New" | cut -d" " -f3-) # Grabs coverage from previous log file.
		rawCountPrev=$(echo $prevCov | awk '{print $1}')

		if [[ prevCov != "" ]] # Check needed so script doesn't fail if prevLogFile doesn't contain coverage info
		then
			# Calculate diff between new and old coverage
			rawCountDiff=$(echo $(expr $rawCount - $rawCountPrev)|tr -d -)
			rawCountAvg=$(expr $rawCount / 2 + $rawCountPrev / 2)
			percentDiff=$(awk -v rcd=$rawCountDiff -v rca=$rawCountAvg 'BEGIN{print 100 * rcd / rca}')

			# Build output string
			output+="$3\nLast updated: $4\nCoverage New: $2\nCoverage Old: $prevCov\nCoverage Diff: $percentDiff%\n\n"
		else
			output+="$3\nLast updated: $4\nCoverage New: $2\n\n"
		fi
	else
		output+="$3\nLast updated: $4\nCoverage New: $2\n\n"
	fi
}

# Function to raise errors
function checkForIssues () {
	# four positional arguments. $1 == tblDate. $2 == tooOld. $3 == tableName. $4 == precentDiff.
	# Dependency: percentDiff, positional argument $4, is expected to be set by outputCovDiff
	# If percent diff is not set (so it retains default empty "" value), then checkForIssues
	# will raise no errors.

	# Raises an error if it's been too long since last update
	if [ $(date -d "$1" +%s) -le $(date -d "$2" +%s) ]
	then
		issueNote+="$db.$3 has not been updated since $1\n"
	fi

	# Raises error if coverage diff between track versions is too large
	if [[ "$4" != "" ]]
	then
		# Round our percentDiff to 3 decimal places.
		percentDiffRounded=$(printf "%.3f\n" "$4")
		if [ $(echo "$percentDiffRounded >= 10" | bc) -ne 0 ]
		then
			issueNote+="Large coverage diff for $db.$3\n"
		fi
	fi
}

##### Parse command-line input #####

OPTIND=1 # Reset is necessary if getopts was used previously in the script.
while getopts "hl:u:bvs" opt
do
	case $opt in
		h)
			showHelp
			exit 0
			;;
		v)
			verboseMode=true
			;;
		b)
			bigBedMode="on"
			;;
		l)
			# Check if directory is valid before attempting to write to it.
			if [ -d "$OPTARG" ]
			then
				newLogDir=$OPTARG
			else
				echo -e "Sorry, directory \"$2\" does not exist."
				echo -e "Check spelling or create this directory and try again.\n"
				exit 1
			fi
			;;
		u)
			logUrl=$OPTARG
			;;
		s)
			overwriteLogUrl=true
			;;
		'?')
			showHelp >&2
			exit 1
			;;
	esac
done

shift "$((OPTIND-1))" # Shift off the options and optional --.

# Check number of required arguments
if [ $# -ne 2 ]
then
        # Output usage message if number of required arguments is wrong
        showHelp >&2
        exit 1
else
        # Set variables with required argument values
        db=$1
        tableName=$2
fi

##### Main Program #####

# Set some variables based on optional input
if [[ "$newLogDir" != "" ]]
then
	# Set logDir if newLogDir is provided
	logDir="$newLogDir"
fi
if [[ $overwriteLogUrl == true ]]
then
	# Overwrite url if option is set
	logUrl=$logDir
fi

# set currLogFile
currLogFile="$db.$tableName.$currDate.$currTime.txt"

# set info for prevLog
prevLogDate=$(ls -Lt $logDir | sed -n /$db.$tableName/p | head -1 | awk -F . '{print $3}')
prevLogTime=$(ls -Lt $logDir | sed -n /$db.$tableName/p | head -1 | awk -F . '{print $4}')

#initialize output string
output="\n$db\n"

if [ -e $logDir/$db.$tableName.$prevLogDate.$prevLogTime.txt ]
then
	prevLogFile="$logDir/$db.$tableName.$prevLogDate.$prevLogTime.txt"
fi

# Set tooOld for different tables
if  [[ "$tableName" == "clinvar" ]]
then
	tooOld=$(date -d "$currDate - 2 months" +%F)
elif [[ "$tableName" == "grcIndcidentDb" ]]
then
	tooOld=$(date -d "$currDate - 6 months" +%F)
else
	tooOld=$(date -d "$currDate - 1 month" +%F)
fi

# Run tests for different tracks 
if [[ $bigBedMode == "on" ]]
then
	# ClinVar has muliple tables
	if [[ $tableName == "clinvar" ]]
	then
		for tbl in clinvarMain clinvarCnv
		do
			# Get file name from beta
			fileName=$(hgsql -Ne "SELECT * FROM $tbl LIMIT 1" $db)
			# Get table update time from beta
			tblDate=$(ssh qateam@hgwbeta "date -d '$(stat -Lc '%y' $fileName)' +%F' '%T")
			MYTEMPFILE=$(mktemp --tmpdir tmp.XXXXXXXXXX.bed)
			# featureBits doesn't work with bigBeds, need to turn into bed first
			ssh qateam@hgwbeta "/usr/local/apache/cgi-bin/utils/bigBedToBed $fileName stdout" > $MYTEMPFILE
			tblCov=$(featureBits -countGaps $db $MYTEMPFILE 2>&1)

			outputCovDiff "$prevLogFile" "$tblCov" "$tbl" "$tblDate"
			# Check for issues with table
			checkForIssues "$tblDate" "$tooOld" "$tbl" "$percentDiff"

			rm -f $MYTEMPFILE
		done
	# Tests for all other bigBed based autopushed tracks (assuming they don't use remote bigBed files)
	else
		fileName=$(hgsql -Ne "SELECT * FROM $tableName LIMIT 1" $db)
		# Get table update time from beta
		tblDate=$(ssh qateam@hgwbeta "date -d '$(stat -Lc '%y' $fileName)' +%F' '%T")
		MYTEMPFILE=$(mktemp --tmpdir tmp.XXXXXXXXXX.bed)
		# featureBits doesn't work with bigBeds, need to turn into bed first
		ssh qateam@hgwbeta "/usr/local/apache/cgi-bin/utils/bigBedToBed $fileName stdout" > $MYTEMPFILE
		tblCov=$(featureBits -countGaps $db $MYTEMPFILE 2>&1)

		outputCovDiff "$prevLogFile" "$tblCov" "$tableName" "$tblDate"
	
		# Check for issues with table
		checkForIssues "$tblDate" "$tooOld" "$tableName" "$percentDiff"

		rm -f $MYTEMPFILE
	fi
# Tests for non-bigBed tracks
else
	# OMIM and ISCA both have a large number of tables
	if [[ $tableName == "omim" ]] || [[ $tableName == "isca" ]]
	then
		for tbl in $(hgsql -Ne "SHOW TABLES LIKE '%$tableName%'" $db) # Grabs list of all omim or isca tables from beta
        	do
			tblDate=$(hgsql -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$db' AND TABLE_NAME='$tbl'")
			# Only some omim tables have coordinates
                	if [[ $tbl == "omimGene2" ]] || [[ $tbl == "omimAvSnp" ]] || [[ $tbl == "omimLocation" ]] || [[ $tableName == "isca" ]]
                	then
                        	tblCov=$(ssh qateam@hgwbeta "featureBits -countGaps $db $tbl 2>&1")

				outputCovDiff "$prevLogFile" "$tblCov" "$tbl" "$tblDate"
				# Check for issues with table
				checkForIssues "$tblDate" "$tooOld" "$tbl" "$percentDiff"

			# Output for tables that don't contain coordinates
			else
				output+="$tbl\nLast updated: $tblDate\n\n"
				# Check for issues with table
				checkForIssues "$tblDate" "$tooOld" "$tbl" ""
			fi
		done
	# Tests for all other table based autopushed tracks
	else
		tblDate=$(hgsql -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$db' AND TABLE_NAME='$tableName'")
		tblCov=$(ssh qateam@hgwbeta "featureBits -countGaps $db $tableName 2>&1")

		outputCovDiff "$prevLogFile" "$tblCov" "$tableName" "$tblDate"

		# Check for issues with table
		checkForIssues "$tblDate" "$tooOld" "$tableName" "$percentDiff"
	fi
fi

# Output results of tests
if [[ $issueNote != "" ]]
then
	#Put URL to log file at end of issue note
	issueNote+="\nSee $logUrl/$currLogFile for more details about these errors.\n\n"

	echo -e $issueNote | tee $logDir/$currLogFile
fi

if [[ $verboseMode == true ]] # True if verboseMode is on
then
	echo -e $output | tee -a $logDir/$currLogFile
else
	echo -e $output >> $logDir/$currLogFile
fi
