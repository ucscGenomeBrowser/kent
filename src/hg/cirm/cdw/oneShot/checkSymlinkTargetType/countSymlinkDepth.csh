#!/bin/tcsh
# first run checkSymlinkTargetType to get input
if ("$1" == "") then
    echo "specify input on commandline"
    exit 1
endif
set fname = "$1"
if (! -e $fname) then
    echo "file specified $fname does not exist."
    exit 1
endif
# count how many times SYMLINK appears on a single line
grep -o -n 'SYMLINK' "$fname" | cut -d : -f 1 | uniq -c | sort -nr | head
# biggest counts should be at the top
