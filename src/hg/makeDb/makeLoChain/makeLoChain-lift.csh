#!/bin/csh
# Name:         makeLoChain-lift
#
# Function:     Lift alignments for liftOver chain.
#               Relies on directory structure created by makeLoChain-align
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-lift.csh,v 1.2 2004/07/14 19:52:28 kate Exp $

if ( $#argv != 3 ) then
    echo "usage: $0 <old-assembly> <new-assembly> <new-liftdir>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2
set newLiftDir = $3

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly

if ( ! -e $blatDir/raw ) then
    echo "Can't find $blatDir/raw"
endif
if ( ! -e $newLiftDir/chr1.lft ) then
    echo "Can't find $newLiftDir/chr1.lft"
endif

cd $blatDir/raw
set fs = `fileServer`
if ( $fs != "" && $fs != $HOST ) then
    echo "Run this on $fs"
    exit 1
endif

foreach i (`cat /cluster/data/$newAssembly/chrom.lst`)
    echo chr$i
    liftUp -pslQ ../psl/chr$i.psl $newLiftDir/chr$i.lft warn chr*_chr$i.psl
    echo done $i
end

echo "Finished lifting alignments to $blatDir/psl"
exit 0

