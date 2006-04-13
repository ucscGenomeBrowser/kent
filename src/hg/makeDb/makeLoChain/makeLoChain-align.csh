#!/bin/csh -ef
# Name:         makeLoChain-align
#
# Function:     Set up blat job for cluster.
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-align.csh,v 1.6 2006/04/06 19:12:28 kate Exp $

if ( $#argv != 4 && $#argv != 5 ) then
    echo "usage: $0 <old-assembly> <old-nibdir> <new-assembly> <new-splitdir> [.ooc-file]"
    exit 1
endif

if ($HOST != 'kk') then
    echo "Must run on host kk"
    exit 1
endif

set oldAssembly = $1
set oldNibDir = $2
set newAssembly = $3
set newSplitDir = $4
set ooc = $5
if ("$ooc" != "") then
    set ooc = '-ooc='$ooc
endif

if (`ls -1 $oldNibDir/*.{nib,2bit} | wc -l` < 1) then
    echo "Can't find any .2bit or .nib files in $oldNibDir"
    exit 1
endif

if (`ls -1 $newSplitDir/*.{nib,fa} | wc -l` < 1) then
    echo "Can't find any .nib or .fa files in $newSplitDir"
    exit 1
endif

set blatDir = /cluster/data/$oldAssembly/bed/blat.$newAssembly.`date +%Y-%m-%d`
echo "Setting up blat in $blatDir"
rm -fr $blatDir
mkdir $blatDir
cd $blatDir
mkdir raw psl run
cd run

echo '#LOOP' > gsub
echo 'blat $(path1) $(path2) {check out line+ ../raw/$(root1)_$(root2).psl} ' \
       '-tileSize=11 '$ooc' -minScore=100 -minIdentity=98 -fastMap' \
  >> gsub
echo '#ENDLOOP' >> gsub


# target
ls -1S $oldNibDir/*.{nib,2bit} > old.lst
# query
ls -1S $newSplitDir/*.{nib,fa} > new.lst

gensub2 old.lst new.lst gsub spec
/parasol/bin/para create spec

set execDir = $0:h
set fs = `fileServer $blatDir`

echo ""
echo "First two lines of para spec:"
head -2 spec
echo ""
echo "DO THIS NEXT:"
echo "    cd $blatDir/run"
echo "    para try, check, push, check, ..."
echo "    ssh $fs"
echo "    $execDir/makeLoChain-lift.csh $oldAssembly $newAssembly <lift-dir>"
echo ""
exit 0

