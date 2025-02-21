#!/bin/bash

# Get date format for file name
today=$(date +"%d-%m-%Y")

# Get the current day of the month for cron report
day=$(date +%d)

ssh qateam@hgdownload2 "find /usr/local/apache/htdocs/ -type f -print0 2>/dev/null | xargs -0 stat" > /hive/users/qateam/hgDowloadFileLists/download2.htdocs.$today.txt
ssh qateam@hgdownload2 "find /data/ -type f -print0 2>/dev/null | xargs -0 stat" > /hive/users/qateam/hgDowloadFileLists/download2.data.$today.txt
ssh qateam@hgdownload1 "find /usr/local/apache/htdocs/ -type f -print0 2>/dev/null | xargs -0 stat" > /hive/users/qateam/hgDowloadFileLists/download1.htdocs.$today.txt
ssh qateam@hgdownload1 "find /data/ -type f -print0 2>/dev/null | xargs -0 stat" > /hive/users/qateam/hgDowloadFileLists/download1.data.$today.txt

# Echo only if it's the 1st
if [[ "$day" == "01" ]]; then
    echo "hgDownload file list complete. See file list here: /hive/users/qateam/hgDowloadFileLists/"
    echo ""
    echo "Latest file e.g. /hive/users/qateam/hgDowloadFileLists/download1.data.$today.txt"
    echo ""
    echo "Preview of file to ensure it is running correctly:"
    head /hive/users/qateam/hgDowloadFileLists/download1.data.$today.txt
fi

gzip /hive/users/qateam/hgDowloadFileLists/*.txt
