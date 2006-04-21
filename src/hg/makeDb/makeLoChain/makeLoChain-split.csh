#!/bin/csh -ef
# Name:         makeLoChain-split
#
# Function:     Split new assembly sequence into 10k chunks for fast mapping 
#		of old assembly to new.  
#
# Author:       angie
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-split.csh,v 1.4 2006/04/21 21:58:12 hiram Exp $

if ( $#argv != 2 ) then
    echo "$0 usage: <new-assembly> <new-nibdir>"
    echo "    Run this on kkr1u00."
    exit 1
endif

set newDb = $1
set newNibDir = $2

if ( ! -e $newNibDir ) then
    echo "Can't find $newNibDir"
    exit 1
endif

if ( $HOST != "kkr1u00" && $HOST != "kkr1u00.kilokluster.ucsc.edu" ) then
    echo "Run this on kkr1u00"
    exit 1
endif

set destDir = /iscratch/i/$newDb/split10k
rm -rf $destDir
mkdir -p $destDir

foreach f ($newNibDir/*.nib)
    set chr = $f:t:r
    set size = `nibSize $f | awk '{print $3;}'`
    echo $chr
    nibFrag -name=$chr -masked $f 0 $size + stdout \
    | faSplit -lift=$destDir/$chr.lft -oneFile size stdin 10000 $destDir/$chr
end

foreach R (2 3 4 5 6 7 8)
    rsync -a --progress /iscratch/i/$newDb/ kkr${R}u00:/iscratch/i/$newDb/
end

set execDir = $0:h
echo ""
echo "DO THIS NEXT:"
echo "    ssh kk"
echo "    $execDir/makeLoChain-align.csh <old-assembly> <old-nibdir> $newDb $destDir [.../11.ooc]"
echo ""
exit 0

