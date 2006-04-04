#!/bin/csh -ef
# Name:         makeLoChain-lift
#
# Function:     Lift alignments for liftOver chain.
#               Relies on directory structure created by makeLoChain-align
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-lift.csh,v 1.8 2006/04/04 20:47:13 kate Exp $

if ( $#argv != 2 ) then
    echo "usage: $0 <old-assembly> <new-assembly>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2
set newLiftDir = /iscratch/i/$newAssembly/split10k

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly.`date +%Y-%m-%d`

if ( ! -e $blatDir/raw ) then
    echo "Can't find $blatDir/raw"
endif

if (`ls -1 $newLiftDir/*.lft | wc -l` < 1) then
    echo "Can't find any .lft files in $newLiftDir"
    exit 1
endif

cd $blatDir/raw
if ($HOST != kkr1u00 && $HOST != kkr1u00.kilokluster.ucsc.edu) then
    echo "Run this on kkr1u00"
    exit 1
endif

foreach chr (`awk '{print $1;}' /cluster/data/$newAssembly/chrom.sizes`)
    echo $chr
    liftUp -pslQ ../psl/$chr.psl $newLiftDir/$chr.lft warn chr*_$chr.psl
end

set execDir = $0:h
echo ""
echo "DO THIS NEXT:"
echo "    ssh kki"
echo "    $execDir/makeLoChain-chain.csh $oldAssembly <$oldAssembly-nibdir> $newAssembly <$newAssembly-nibdir>"
echo ""
exit 0

