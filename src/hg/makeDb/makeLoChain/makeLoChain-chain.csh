#!/bin/csh -ef
# Name:         makeLoChain-chain
#
# Function:     Setup cluster job to chaining alignments for liftOver chain
#               Relies on directory structure created by makeLoChain-lift
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-chain.csh,v 1.6 2006/05/15 20:42:19 hiram Exp $

if ( $#argv != 4 ) then
    echo "usage: $0 <old-assembly> <old-nibdir> <new-assembly> <new-nibdir>"
    echo "    Run this on a cluster hub: kk, pk, or kki."
    exit 1
endif

if ( $HOST != 'kk' && $HOST != 'pk' && $HOST != 'kki' ) then
    echo "Must run on host kk, pk, or kki"
    exit 1
endif

set oldAssembly = $1
set oldNibDir = $2
set newAssembly = $3
set newNibDir = $4

if (`ls -1 $oldNibDir/*.nib | wc -l` < 1) then
    echo "Can't find any .nib files in $oldNibDir"
    exit 1
endif

if (`ls -1 $newNibDir/*.nib | wc -l` < 1) then
    echo "Can't find any .nib files in $newNibDir"
    exit 1
endif

set prefix = /cluster/data/$oldAssembly/bed/blat.$newAssembly
set blatDir = `ls -td $prefix.20* | head -1`
echo "using dir $blatDir"
cd $blatDir
rm -fr chainRun chainRaw
mkdir chainRun chainRaw
cd chainRun

cat > template << EOF
#LOOP
axtChain -verbose=0 -linearGap=medium -psl \$(path1) $oldNibDir $newNibDir {check out line+ ../chainRaw/\$(root1).chain}
#ENDLOOP
EOF

ls -1S ../psl/*.psl > in.lst
gensub2 in.lst single template jobList
para create jobList

set execDir = $0:h
set fs = `fileServer $blatDir`

echo ""
echo "First two lines of para jobList:"
head -2 jobList
echo ""
echo "DO THIS NEXT:"
echo "    cd $blatDir/chainRun"
echo "    para try, check, push, check, ..."
echo "    ssh $fs"
echo "    $execDir/makeLoChain-net.csh $oldAssembly $newAssembly"
echo ""
exit 0

