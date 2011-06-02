#!/bin/csh -ef

# encodeExpBackup - copy current encodeExp table and history to backup table, removing
#                       backup table from same day of previous week.  Intended for nightly 
#                       backup via cron

set date = `date +%Y%m%d`
set day = `date +%a`
set current = encodeExp_${date}_${day}_Backup
set prev = `hgsql hgFixed -N -e "show tables like 'encodeExp_%_${day}_Backup'" | head -1`

if ("$prev" != "$current") then
    encodeExp copy -table=encodeExp $current
    encodeExp drop -table=$prev
else
    encodeExp copy -table=encodeExp ${current}New
    encodeExp drop -table=$current
    encodeExp rename -table=${current}New $current
endif
