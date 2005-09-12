#!/bin/csh -ef
# Name:         makeLoChain-chain
#
# Function:     Setup cluster job to chaining alignments for liftOver chain
#               Relies on directory structure created by makeLoChain-lift
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-chain.csh,v 1.3 2005/09/12 21:58:16 kate Exp $

if ( $#argv != 4 ) then
    echo "usage: $0 <old-assembly> <old-nibdir> <new-assembly> <new-nibdir>"
    echo "    Run this on a cluster hub: kk, kk9 or kki."
    exit 1
endif

if ( $HOST != 'kk' && $HOST != 'kk9' && $HOST != 'kki' ) then
    echo "Must run on host kk, kk9 or kki"
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

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly.`date +%Y-%m-%d`
cd $blatDir
rm -fr chainRun chainRaw
mkdir chainRun chainRaw
cd chainRun

cat > gsub << EOF
#LOOP
axtChain -verbose=0 -psl \$(path1) $oldNibDir $newNibDir {check out line+ ../chainRaw/\$(root1).chain}
#ENDLOOP
EOF

ls -1S ../psl/*.psl > in.lst
gensub2 in.lst single gsub spec
para create spec

set execDir = $0:h
set fs = `fileServer $blatDir`

echo ""
echo "First two lines of para spec:"
head -2 spec
echo ""
echo "DO THIS NEXT:"
echo "    cd $blatDir/chainRun"
echo "    para try, check, push, check, ..."
echo "    ssh $fs"
echo "    $execDir/makeLoChain-net.csh $oldAssembly $newAssembly"
echo ""
exit 0

