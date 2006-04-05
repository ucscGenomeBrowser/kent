#!/bin/csh -ef
# Name:         makeLoChain-load
#
# Function:     Load a liftOver chain into database and save to download dir
#               Relies on directory structure created by makeLoChain-net
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-load.csh,v 1.3 2006/04/05 19:06:10 kate Exp $

if ( $#argv != 2 ) then
    echo "usage: $0 <old-assembly> <new-assembly>"
    exit 1
endif

if ( $HOST != hgwdev ) then
    echo "Must run on host hgwdev"
    exit 1
endif

set oldAssembly = $1
set newAssembly = $2

set upperNewAssembly = $newAssembly:u
set filename = ${oldAssembly}To${upperNewAssembly}.over.chain
set chainFile = /cluster/data/$oldAssembly/bed/liftOver/$filename.gz

if ( ! -e $chainFile ) then
    echo "Can't find $chainFile"
    exit 1
endif

# link to download area
set downloadDir = /usr/local/apache/htdocs/goldenPath/$oldAssembly
cd $downloadDir
mkdir -p liftOver
ln -s $chainFile liftOver

# load into database
mkdir -p /gbdb/$oldAssembly/liftOver
rm -f /gbdb/$oldAssembly/liftOver/$filename
ln -s $chainFile /gbdb/$oldAssembly/liftOver
hgAddLiftOverChain $oldAssembly $newAssembly

echo "Finished loading $filename"
echo "Now, add link for $downloadDir/liftOver/$filename to hgLiftOver"
exit 0

