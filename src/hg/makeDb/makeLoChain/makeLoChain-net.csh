#!/bin/csh -ef
# Name:         makeLoChain-net
#
# Function:     Create alignment net and extract liftOver chains
#               Relies on directory structure created by makeLoChain-chain
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-net.csh,v 1.3 2006/04/05 19:06:11 kate Exp $

if ( $#argv != 2 ) then
    echo "usage: <old-assembly> <new-assembly>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2
set prefix = /cluster/data/$oldAssembly/bed/blat.$newAssembly
set blatDir = `ls -td $prefix.20* | head -1`
echo "using dir $blatDir"

cd $blatDir

if ( ! -e $blatDir/chainRaw ) then
    echo "Can't find $blatDir/chainRaw"
    exit 1
endif

set fs = `fileServer $blatDir`
if ( $HOST != $fs ) then
    echo "Run this on $fs"
    exit 1
endif

rm -fr chain
mkdir chain
echo "Sorting chains"
chainMergeSort chainRaw/*.chain | chainSplit chain stdin
rm -fr net
mkdir net
cd chain
echo "Creating net "
foreach i (*.chain)
    echo $i:r
    chainNet $i /cluster/data/$oldAssembly/chrom.sizes \
            /cluster/data/$newAssembly/chrom.sizes ../net/$i:r.net /dev/null
    echo done $i
end

rm -fr ../over
mkdir ../over
foreach i (*.chain)
    echo $i:r
    netChainSubset ../net/$i:r.net $i ../over/$i
end

set upperNewAssembly = $newAssembly:u
set filename = ${oldAssembly}To${upperNewAssembly}.over.chain
set destDir = /cluster/data/$oldAssembly/bed/liftOver
mkdir -p $destDir
chainMergeSort ../over/*.chain > $destDir/$filename
gzip $destDir/$filename

set execDir = $0:h
echo ""
echo "Created $destDir/$filename.gz"
echo "DO THIS NEXT:"
echo "    ssh hgwdev"
echo "    $execDir/makeLoChain-load.csh $oldAssembly $newAssembly"
echo ""
exit 0

