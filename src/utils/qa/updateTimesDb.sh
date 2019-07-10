#!/bin/bash

# quit if something within the script fails
set -beEu -o pipefail
source `which qaConfig.bash`
umask 002

############################
# 
#  02-17-16 -- Matt Speir
#
#  This script will output a the update
#  times for all of the tables in a single
#  database on dev and beta. One can also
#  specify a different database on beta
#  so that table update times can be
#  compared between different databases on
#  different machines.
#
#  This script is intended to replace
#  updateTimesDb.csh
#
############################

##### Variables #####
# Set by command-line options
dbDev=""
dbBeta=""
outputFiles=false

##### Functions #####

showHelp() {
cat << EOF

Usage: `basename $0` [-hf] [-d DATABASE DEV] [-b DATABASE BETA]

	-h                  Display this help and exit
	-d DATABASE DEV     Database to check on Dev, e.g. hg19 or hg38.
	-b DATABASE BETA    Database to check on Beta.
	-f 		    Output lists of tables on Dev and Beta into 
			    files.

Shows table update times for an entire database on Dev and Beta. If you wish to
compare databases between the machines, use the -b option to specify the
database on Beta.

For example, to compare the same assembly on Dev and Beta

	updateTimesDb.sh -d ce10

Or, for example to compare two different databases on Dev and Beta

	updateTimesDb.sh -d ce6 -b ce10
 
Output is formatted as a table:

             dev db	beta db
tableName    <time>	<time>

If -f is used, the list of tables on Dev is output to <db>.tables and the list
of tables on Beta is output to <db>.beta.tables.

Notes: 
- If a table isn't present on a machine, then you will see a "." in place
  of a time
- trackDb and hgFindSpec tables are excluded from the output

EOF
}

##### Parse command-line input #####

OPTIND=1 # Reset is necessary if getopts was used previously in the script.
	 # It is a good idea to make this local in a function.
while getopts "hd:b:f" opt
do                     
        case $opt in   
                h)     
                        showHelp
                        exit 0
                        ;;
                d)     
                        dbDev=$OPTARG
			# Check if dbBeta is empty before trying to set it
			if [[ $dbBeta == "" ]]
			then
				dbBeta=$dbDev
			fi
			;;
                b)     
                        dbBeta=$OPTARG
                        ;;
                f)     
                        outputFiles=true
                        ;;
                '?')   
                        showHelp >&2
                        exit 1
                        ;;
        esac
done
# Check if no command line options were supplied
if [ $OPTIND -eq 1 ]
then
	showHelp >&2
	exit 1
fi

shift "$((OPTIND-1))" # Shift off the options and optional --.

##### Main Program #####

# Take tables for each DB from dev and beta and store as seperate arrays
if [[ $outputFiles == true ]]
then
	# If "-f" is used output list of tables on Dev and Beta to a file.
	# These files of table names are used by other programs, 
	# including proteins.csh.
	tablesDev=($(hgsql -Ne "SHOW TABLES" $dbDev | sort | \
		tee $dbDev.tables ))
	tablesBeta=($(hgsql -h $sqlbeta -Ne "SHOW TABLES" $dbBeta | sort | \
		tee $dbBeta.beta.tables ))
else 
	tablesDev=($(hgsql -Ne "SHOW TABLES" $dbDev | sort))
	tablesBeta=($(hgsql -h $sqlbeta -Ne "SHOW TABLES" $dbBeta | sort))
fi

# Combine tables from Dev and Beta, then sort and make list unique
tablesSortedUnique=$(echo "$(echo ${tablesDev[@]} ${tablesBeta[@]})" | \
	tr ' ' '\n' | sort -u | grep -v 'hgFindSpec\|trackDb' | tr '\n' ' ')

output=". DEV BETA\ntableName $dbDev $dbBeta\n"

# if there's errors, the *** is causing wildcard expansion, so disable globbing
set -f

for tbl in $(echo ${tablesSortedUnique[@]})
do
	# Underscores added between date and time to that way "column" command later on works correctly
        devUpdate=$(hgsql -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$dbDev' AND TABLE_NAME='$tbl'" \
		| sed 's/ /_/g')
        betaUpdate=$(hgsql -h $sqlbeta -Ne "SELECT UPDATE_TIME FROM information_schema.tables WHERE TABLE_SCHEMA='$dbBeta' AND TABLE_NAME='$tbl'" \
		| sed 's/ /_/g')

	# Can't have have completely empty entries or "column" won't work
        if [[ $devUpdate == "" ]]
        then   
                devUpdate="."
        elif [[ $betaUpdate == "" ]]
        then   
                betaUpdate="."
        fi

	# Variable to hold error/diff marking
	error=""

	# Check to see if table times match on Dev and Beta
	if [[ $devUpdate != $betaUpdate ]] && [[ $devUpdate != "." ]] && [[ $betaUpdate != "." ]]
	then
		# If they don't match, mark diff lines with ***
		error="***"
	fi

	# Update our output string with new info
        output+="$tbl $devUpdate $betaUpdate $error\n"
done

# Output results to command line 
# Final sed is to remove underscores inserted as part of last step
echo -e $output | column -t -s' ' | sed 's:\([0-9]\)\(_\)\([0-9]\):\1 \3:g'
