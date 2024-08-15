#!/bin/bash

# Checks the last TABLE STATUS dump

# Get the current date in the format "YYYY-MM-DD"
current_date=$(date +%Y-%m-%d)

# Modify the date for when the cron runs
check_dumped_date=$(date -d "$current_date - 1 day" +%Y-%m-%d)

# Replace '-' to '.' to match the date output for the last TABLE STATUS dump 
check_dumped_date="rr: ${check_dumped_date//-/.}"

# Get the last TABLE STATUS dump 
dumped_date="rr: $(ls -1 /hive/data/outside/genbank/var/tblstats/hgnfs1/ | tail -1)" 

# If the TABLE STATUS dump date does not match the expected date, outputs a message:
# "The RR TABLE STATUS files have not been recently dumped. The latest RR dump was 2024.03.31."
if [ "$check_dumped_date"  = "$dumped_date"  ]; then
    :
else
    echo "The RR TABLE STATUS files have not been recently dumped. The latest RR dump was $(ls -1 /hive/data/outside/genbank/var/tblstats/hgnfs1/ | tail -1)."
fi
