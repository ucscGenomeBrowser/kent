#!/bin/csh
# Name:         makeLoChain-net
#
# Function:     Create alignment net and extract liftOver chains
#               Relies on directory structure created by makeLoChain-chain
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-net.csh,v 1.1 2004/04/20 20:28:25 kate Exp $

if ( $#argv != 2 ) then
    echo "$0 usage: <old-assembly> <new-assembly>"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly
cd $blatDir

if ( ! -e $blatDir/chainRaw ) then
    echo "Can't find $blatDir/chainRaw"
    exit 1
endif

set fs = `fileServer`
if ( $fs != "" && $fs != $HOST ) then
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

cat ../over/*.chain > ../over.chain
mkdir -p /cluster/data/$oldAssembly/bed/bedOver
set upperNewAssembly = $newAssembly:u
set filename = ${oldAssembly}To${upperNewAssembly}.over.chain
cp ../over.chain /cluster/data/$oldAssembly/bed/bedOver/$filename

echo "Finished creating $filename"
exit 0

