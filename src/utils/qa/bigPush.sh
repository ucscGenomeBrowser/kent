#!/bin/bash
set -beEu -o pipefail
source `which qaConfig.bash`
umask 002

################################
#
#  04-15-16
#  Matt Speir
#
#  Designed to make large pushes 
#  of many tables for many assemblies
#  easier.
#
################################


##### Variables #####

dbList=""
tableList=""
verboseMode=""
outputMode=""

##### Functions #####

showHelp() {
cat << EOF
 
Usage: $0 [-h] [-d DATABASE(S)] [-t TABLE(S)] [-v VERBOSITY LEVEL] 
 
        -h                  Display this help and exit.
        -d DATABASE(S)	    A single database or list of databases for which
			    table(s) will be pushed. This list can be a file.
        -t TABLE(S)	    A single table or list of tables to be pushed.
			    List of tables can be a file.
        -v VERBOSITY LEVEL  Output details of push; 1 for results to stdout,
			    2 for results to stdout and file.

Push a table or list of tables from Dev to Beta for a single database or list
of databases.

If you have multiple databases (and/or tables), they can be input in a few
different ways. They can be either in a file or in a space-separated list
enclosed in quotes. For example:

$0 -d "ce11 hg19 gorGor3" -t refGene

A list of tables can be pushed in a similar way.

If verbose mode is set to "2" with -v 2, then the output will be sent to
stdout and to a file. The file name will be push.<YYYY-MM-DD_hhmm>, where
the part after "push." is Year-Month-Day_HourMinute.

EOF
}

##### Parse command-line input #####
                       
OPTIND=1 # Reset is necessary if getopts was used previously in the script.  It is a good idea to make this local in a function.
while getopts "hd:t:v:" opt
do
        case $opt in
                h)
                        showHelp
                        exit 0
                        ;;
                d)
                        dbList=$OPTARG
                        ;;
                t)     
                        tableList=$OPTARG
                        ;;
                v)
			# Validate verbose option input
			# Fails if input is not one of two supported options
			if [[ $OPTARG != 1 ]] && [[ $OPTARG != 2 ]]
			then
				echo -e "Verbose mode should be 1 or 2. Or if no output is wanted, not used at all."
				exit 1
			else
                        	verboseMode=$OPTARG
			fi
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

# Adjust output mode based on specified verbose mode 
if [[ $verboseMode == 1 ]]
then
	# Will cause output to be sent to stdout
	outputMode=""
elif [[ $verboseMode == 2 ]]
then
	# Output goes to stdout and file 
	outputMode=" | tee -a push.$(date +%F_%H%M)"
else
	# if no verbose mode specified just send output to /dev/null
	outputMode="> /dev/null"
fi


# First check if dbList is a file.
# If not, skips down to "# Here"
if [[ -e $dbList ]]
then
	# Loop through our dbList file
	for db in $(cat $dbList)
	do
		# Now check if tableList is a file
		if [[ -e $tableList ]]
		then
			# If tableList was a file, loop through contents
			for tbl in $(cat $tableList)
			do
				# Build command with proper database, table name, and output mode.
				command="sudo mypush $db $tbl mysqlbeta $outputMode"
				# Run command using bash
				echo $command | bash
			done
		# Or, if tableList was not a file, push table(s) to beta
		else
			for tbl in $(echo $tableList)
			do
				command="sudo mypush $db $tbl mysqlbeta $outputMode"
				echo $command | bash
			done
		fi
	done
# Here. 
# Conditional excutes if dbList input was not a file.
else 
	for db in $(echo $dbList)
	do
		# Similar to above, following conditionals and for loops
		# push tables from dev to beta if dbList was not a file
		if [[ -e $tableList ]]
		then
			for tbl in $(cat $tableList)
			do
				command="sudo mypush $db $tbl mysqlbeta $outputMode"
				echo $command | bash
			done
		else
			for tbl in $(echo $tableList)
			do
				command="sudo mypush $db $tbl mysqlbeta $outputMode"
				echo $command | bash
			done
		fi
	done
fi
