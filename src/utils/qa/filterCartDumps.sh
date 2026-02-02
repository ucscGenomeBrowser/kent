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

# Bash series of commands that will save region size, load time, session date, and session cart dump for each session
# Output example: 192728 2.345 2024-01-15-14:30 http://genome.ucsc.edu/cgi-bin/cartDump?hgS_doLoadUrl=submit&hgS_loadUrlName=...
for url in  $(grep -o 'href=\"[^\"]*\"'  "$index_file" | awk -F '"' '{print $2}' \
            | sed "s|/hgTracks|/cartDump|"); do 
    start_time=$(date +%s.%N)
    response=$(curl -Ls "$url")
    end_time=$(date +%s.%N)
    load_time=$(echo "$end_time - $start_time" | bc)
    
    # Extract the session date from the index.html file
    filename=$(echo "$url" | grep -o '[0-9]*\._genome_[^&]*\.txt')
    session_date=$(grep "$filename" "$index_file" | grep -o '<td>[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]:[0-9][0-9]</td>' | sed 's/<td>//;s/<\/td>//')
    
    echo "$response" | grep 'position ' \
        | awk '{$1=$1}1' OFS="\t" | cut -f2 | tr ":" "\t" | tr "-" "\t" \
        | awk -v time="$load_time" -v session="$url" -v date="$session_date" '{ print $3-$2"\t"time"\t"date"\t"session }'
done > "$regionSessions_file"

# Filter sessions by region size and save with load time and date
# regionLimit=10000000 (10 Mb)
while IFS=$'\t' read -r region_size load_time session_date url || [[ -n "$region_size" ]]; do
    # Skip empty lines or lines where region_size is not a number
    if [[ "$region_size" =~ ^[0-9]+$ ]] && [ "$region_size" -lt 10000000 ]; then
        echo -e "$load_time\t$session_date\t$url" | sed "s|cgi-bin/cartDump|cgi-bin/hgTracks|" >> "$filter_file"
    fi
done < "$regionSessions_file"

# Sort by load time (descending) and get the 10 slowest sessions
echo "The 10 slowest sessions (by load time):" > "$cron_output"
echo "Load Time | Session Created (expire after 2 weeks) | Session URL" >> "$cron_output"
echo "----------|---------------------------------------|------------" >> "$cron_output"
sort -t$'\t' -k1 -rn "$filter_file" | head -n 10 | awk -F'\t' '{printf "%.3fs\t%s\t%s\n", $1, $2, $3}' >> "$cron_output"

echo "" >> "$cron_output"
# Get 20 random sessions
echo "20 random sessions:" >> "$cron_output"
echo "Load Time | Session Created (expire after 2 weeks) | Session URL" >> "$cron_output"
echo "----------|---------------------------------------|------------" >> "$cron_output"
tail -n +11 "$filter_file" | shuf | head -n 20 | awk -F'\t' '{printf "%.3fs\t%s\t%s\n", $1, $2, $3}' >> "$cron_output"

rm /hive/users/qateam/cronCartDumps/regionSessions.txt
rm /hive/users/qateam/cronCartDumps/filter_file

cat "$cron_output"
echo ""
echo "Archive for the most recent long-running sessions can be found here: https://genecats.gi.ucsc.edu/qa/qaCrons/cronCartDumps/index.html"
