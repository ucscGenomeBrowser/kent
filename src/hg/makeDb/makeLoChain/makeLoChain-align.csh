#!/bin/csh
# Name:         makeLoChain-align
#
# Function:     Set up blat job for cluster.
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-align.csh,v 1.2 2004/07/14 19:52:28 kate Exp $

if ( $#argv != 4 ) then
    echo "usage: $0 <old-assembly> <old-nibdir> <new-assembly> <new-splitdir>"
    exit 1
endif

if ( $HOST != kk ) then
    echo "Must run on host kk"
    exit 1
endif

set oldAssembly = $1
set oldNibDir = $2
set newAssembly = $3
set newSplitDir = $4

if ( ! -e $oldNibDir/chr1.nib ) then
    echo "Can't find $oldNibDir/chr1.nib"
    exit 1
endif

if ( ! -e $newSplitDir/chr1.fa ) then
    echo "Can't find $newSplitDir/chr1.fa"
    exit 1
endif

cd /cluster/data/$oldAssembly/bed
set blatDir = blat.$newAssembly.`date +%Y-%m-%d`
echo "Setting up blat in $blatDir"
rm -fr $blatDir
mkdir -p $blatDir
cd $blatDir
mkdir -p raw psl run
cd run
cat > gsub << 'EOF'
#LOOP
    blat $(path1) $(path2) {check out line+ ../raw/$(root1)_$(root2).psl} -tileSize=11 -ooc=/cluster/bluearc/hg/h/11.ooc -minScore=100 -minIdentity=98 -fastMap
#ENDLOOP
'EOF'

# target
ls -1S $oldNibDir/*.nib > old.lst
# query
ls -1S $newSplitDir/*.fa > new.lst

gensub2 old.lst new.lst gsub spec
para create spec

head -2 spec
echo "Created parasol job in bed/$blatDir/run"
echo "Now, run the cluster job on kk, then run makeLoChain-lift on the fileserver"
exit 0

