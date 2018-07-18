#!/bin/bash

##### Functions #####

showHelp() {
cat << EOF

Creates symlinks to error or access logs in current directory for specified time frame
 
Usage: `basename $0` startDate endMonth logType

    Required arguments:

      startDate    Which log file to start at, must be date for log file that exists
      endMonth     Which month you want to end at, should be a number between 1 and 12
      logType      Whether you want to make a directory of error_log or access_log files

    Example:
      `basename $0` 20180107 4 error
EOF
}

##### Parse command-line input #####

if [ "$#" -ne 3 ]
then
    # Output usage message if number of required arguments is wrong
    showHelp >&2
    exit 1   
else
    # Set variables with required argument values
    startDate=$1 # Ensure it's one of the ones that we actually have a log for
    endMonth=$2 # 
    errorAccess=$3 # Should be "error" or "access"
fi

if [[ $errorAccess != "error" ]] && [[ $errorAccess != "access" ]]
then
    echo "Sorry, logType must be either 'error' or 'access'. You entered '$3'."
    echo "Note these terms are case-sensitive."
    exit 1;
fi

##### Variables #####

newDate=$(date -d "$startDate" +%Y%m%d)
year=$(date -d "$startDate" +%Y)

##### Main Program #####

while [ $(date -d "$newDate" +%m) -le $(($endMonth)) ] && [ $(date -d "$newDate" +%Y) -eq $(($year)) ] 
do
    day=$newDate
    # Make sym links for RR machines
    for server in RR euroNode asiaNode
    do
        if [[ $server == "RR" ]]
        then
            for mach in hgw1 hgw2 hgw3 hgw4 hgw5 hgw6
            do
                ln -s /hive/data/inside/wwwstats/$server/$year/$mach/${errorAccess}_log.$day.gz $mach.${errorAccess}_log.$day.gz
            done
        else
            ln -s /hive/data/inside/wwwstats/$server/$year/$mach/${errorAccess}_log.$day.gz $server.${errorAccess}_log.$day.gz
        fi
    done
 
    newDate=$(date -d "$day +7days" +%Y%m%d)
    echo $newDate
done

# Remove any accidentally created broken links
find -L $DIR -maxdepth 1 -type l -delete
