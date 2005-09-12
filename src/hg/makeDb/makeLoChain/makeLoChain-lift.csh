#!/bin/csh -ef
# Name:         makeLoChain-lift
#
# Function:     Lift alignments for liftOver chain.
#               Relies on directory structure created by makeLoChain-align
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-lift.csh,v 1.4 2005/09/12 21:58:16 kate Exp $

if ( $#argv != 3 ) then
    echo "usage: $0 <old-assembly> <new-assembly> <new-liftdir>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2
set newLiftDir = $3

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly.`date +%Y-%m-%d`

if ( ! -e $blatDir/raw ) then
    echo "Can't find $blatDir/raw"
endif

if (`ls -1 $newLiftDir/*.lft | wc -l` < 1) then
    echo "Can't find any .lft files in $newLiftDir"
    exit 1
endif

cd $blatDir/raw
set fs = `fileServer`
if ( $fs != "" && $fs != $HOST && $HOST != "kolossus") then
    echo "Run this on $fs or kolossus"
    exit 1
endif

foreach n (`ls /cluster/data/$newAssembly/nib`)
    set c = $n:r
    echo $c
    liftUp -pslQ ../psl/$c.psl $newLiftDir/$c.lft warn chr*_$c.psl
    echo done $c
end

set execDir = $0:h
echo ""
echo "DO THIS NEXT:"
echo "    ssh kki"
echo "    $execDir/makeLoChain-chain.csh $oldAssembly <$oldAssembly-nibdir> $newAssembly <$newAssembly-nibdir>"
echo ""
exit 0

