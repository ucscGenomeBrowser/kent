#!/bin/csh
# Name:         makeLoChain-chain
#
# Function:     Setup cluster job to chaining alignments for liftOver chain
#               Relies on directory structure created by makeLoChain-lift
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-chain.csh,v 1.1 2004/04/20 20:28:25 kate Exp $

if ( $#argv != 4 ) then
    echo "usage: $0 <old-assembly> <old-nibdir> <new-assembly> <new-nibdir>"
    exit 1
endif

if ( $HOST != kk ) then
    echo "Must run on host kk"
    exit 1
endif

set oldAssembly = $1
set oldNibDir = $2
set newAssembly = $3
set newNibDir = $4

if ( ! -e $oldNibDir/chr1.nib ) then
    echo "Can't find $oldNibDir/chr1.nib"
    exit 1
endif

if ( ! -e $newNibDir/chr1.nib ) then
    echo "Can't find $newNibDir/chr1.nib"
    exit 1
endif

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly
cd $blatDir
rm -fr chainRun chainRaw
mkdir -p chainRun chainRaw
cd chainRun

cat > gsub << EOF
#LOOP
axtChain -psl \$(path1) $oldNibDir $newNibDir {check out line+ ../chainRaw/\$(root1).chain}
#ENDLOOP
EOF

ls -1S ../psl/*.psl > in.lst
gensub2 in.lst single gsub spec
para create spec

head -2 spec
echo "Created parasol job in $blatDir/chainRun"
echo "Now, run the cluster job on kk, then run makeLoChain-net on the fileserver"
exit 0

