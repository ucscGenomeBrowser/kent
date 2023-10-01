#!/bin/bash

# Program Header
# Name:   Gerardo Perez
# Description: A program that filters longSessionCartDumps.sh cart dumps by region size limit, then
#              appends the first 10 slowest sessions and 20 random sessions to a file
#
# filterCartDumps.sh
#
index_file="/usr/local/apache/htdocs-genecats/qa/qaCrons/cronCartDumps/index.html"
regionSessions_file="/hive/users/qateam/cronCartDumps/regionSessions.txt"
filter_file="/hive/users/qateam/cronCartDumps/filter_file"
cron_output="/usr/local/apache/htdocs-genecats/qa/qaCrons/cron_output"

# Bash series of commands that will save both region size and session cart dump for each session into file
# Output example: 192728	http://genome.ucsc.edu/cgi-bin/cartDump?hgS_doLoadUrl=submit&hgS_loadUrlName=...
for url in  $(grep -o 'href=\"[^\"]*\"'  "$index_file" | awk -F '"' '{print $2}' \
            | sed "s|/hgTracks|/cartDump|"); do curl -s $url  | grep 'position ' \
            |  awk '{$1=$1}1' OFS="\t" | cut -f2 | tr ":" "\t" | tr "-" "\t" \
            |  awk ' { print $3 -$2"\t"session }' session=$url; \
            done > "$regionSessions_file"

# Bash series of commands that will save the session cart dump as viewable link if it passes the region size limit
#regionLimit=5000000
while IFS= read -r line || [[ -n "$line" ]]; do
    number=$(echo "$line" | awk '{print $1}')  # Extract the first column
    if [ "$number" -lt 5000000 ]; then
        echo "$line" | awk '{ print $2}' | sed "s|cgi-bin/cartDump|cgi-bin/hgTracks|"  >> "$filter_file"
    fi
done < "$regionSessions_file"

# Get the first 10 slowest sessions and append it to a file
echo The 10 slowest sessions > "$cron_output"
head -n 10 "$filter_file" >> "$cron_output"

echo "" >> "$cron_output"
# Get 20 random sessions and append it to a file
echo "20 random sessions" >> "$cron_output"
tail -n +11 "$filter_file" | shuf | head -n 20 >> "$cron_output"
rm /hive/users/qateam/cronCartDumps/regionSessions.txt
rm /hive/users/qateam/cronCartDumps/filter_file
