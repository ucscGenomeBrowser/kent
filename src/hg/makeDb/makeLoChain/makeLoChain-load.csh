#!/bin/csh -ef
# Name:         makeLoChain-load
#
# Function:     Load a liftOver chain into database and save to download dir
#               Relies on directory structure created by makeLoChain-net
#
# Author:       kate
#
# $Header: /projects/compbio/cvsroot/kent/src/hg/makeDb/makeLoChain/makeLoChain-load.csh,v 1.2 2004/09/18 00:18:57 angie Exp $

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
set chainFile = /cluster/data/$oldAssembly/bed/bedOver/$filename

if ( ! -e $chainFile ) then
    echo "Can't find $chainFile"
    exit 1
endif

# save to download area
set downloadDir = /usr/local/apache/htdocs/goldenPath/$oldAssembly
cd $downloadDir
mkdir -p liftOver
gzip -c $chainFile > liftOver/$filename.gz

# load into database
mkdir -p /gbdb/$oldAssembly/liftOver
rm -f /gbdb/$oldAssembly/liftOver/$filename
ln -s /cluster/data/$oldAssembly/bed/bedOver/$filename \
                /gbdb/$oldAssembly/liftOver
hgAddLiftOverChain $oldAssembly $newAssembly

echo "Finished loading $filename"
echo "Now, add download link for $downloadDir/liftOver/${filename}.gz to hgLiftOver"
exit 0

