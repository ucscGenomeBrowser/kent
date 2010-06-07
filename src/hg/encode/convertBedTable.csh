#!/bin/csh -f

# convertTable <fromDb> <toDb> <table>
# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/convertBedTable.csh,v 1.5 2005/11/29 22:08:07 kate Exp $

set encodeBin = /cluster/data/encode/bin/scripts

set usage = "usage: convertBedTable <fromDb> <toDb> <table> <bedFieldCt> [-chain <chainfile>]"

if ($#argv != 4 && $#argv != 6) then
    echo $usage
    exit 1
endif

set from = $1
set to = $2
set table = $3
set fields = $4

if ($#argv == 6) then
    if ($5 != "-chain") then
        echo $usage
        exit 1
    endif
    set chain = $5
else
    set chain = `echo "SELECT path FROM liftOverChain WHERE fromDb='$from' AND toDb='$to'" | hgsql -N -s hgcentraltest`
endif

# check if table already exists (don't overwrite it)
hgsql -N -s $to -e "desc $table" >& /dev/null
if ($status == 0) then
    echo "Table ${to}:$table already exists -- remove it first"
    exit 1
endif

# use mysqldump to generate .sql w/ schema, and .txt with data
hgsqldump  -T . $from $table
if ($status == 1) then
    echo "Error dumping table $from $table"
    exit 1
endif
echo "Creating $db $table.sql and $table.txt"
wc -l $table.txt

# determine if table has bin field
hgsql -N -s $from -e "desc $table" | head -1 | grep -q "^bin"
if ($status == 0) then
    set opts = "-hasBin"
else
    set opts = ""
endif

# convert data coordinates
liftOver $table.txt $opts -tab -bedPlus=$fields $chain $table.tab $table.unmapped
if ($status != 0) then
    echo "Liftover error: converting $table.txt $opts -bedPlus=$fields"
    exit 1
endif
wc -l $table.tab $table.unmapped

# create table
hgsql $to < $table.sql

# load into database
echo "LOAD DATA local INFILE '$table.tab' INTO TABLE $table" | hgsql $to
checkTableCoords $to $table
set old = `hgsql $from -N -s -e "SELECT COUNT(*) FROM $table"`
set new = `hgsql $to -N -s -e "SELECT COUNT(*) FROM $table"`
echo "$table	$from $old   $to $new"

