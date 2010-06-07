#!/bin/csh -ef

# regionToChromFiles - create chrom files from ENCODE region files
# Expects filenames <region>.<ext> in current directory

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/regionToChromFiles.csh,v 1.1 2005/12/01 00:32:35 kate Exp $

set usage = "usage: regionToChromFiles <db> <file-extension> <outDir>"

if ($#argv != 3) then
    echo $usage
    exit 1
endif

set db = $1
set ext = $2
set outDir = $3
mkdir -p $outDir
set regions = `hgsql $db -N -e \
    "select chrom,name from encodeRegions order by chrom, chromStart" | \
    sed 's/	EN/.EN/g'`
foreach s ($regions)
    set c = $s:r
    set r = $s:e
    echo "$c $r"
    cat $r.$ext >> $outDir/$c.$ext
end

