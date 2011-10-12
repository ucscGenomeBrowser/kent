#!/bin/tcsh -efx

#
# compareModifiedFileSizes.csh: look at the sizes of recently created / modified files,
#     and compare their sizes to those of (comparable) earlier versions of the same
#     files.
#
echo $#
if ( $#argv < 2 ) then
    echo "compareModifiedFileSizes.csh: compare the size of recently-modified files against previous versions" 
    echo "usage"
    echo " compareModifiedFileSizes.csh <oldDir> <newDir> [ctimeArg]"
    echo "options:"
    echo "- oldDir: the directory with the old versions of the files."
    echo "- newDir: the directory with the new versions of the file. "
    echo "- [ctimeArg]: argument for ctime to indicate files of what age to select (default: 1 day old)"
else
    set tmpDir = /hive/scratch/compareModifiedFileSizes.$$
    mkdir $tmpDir
    set ctimeArg = -1
    if ( $#argv > 2 ) then
        set ctimeArg = $3
    endif
    pushd $2
    find . -type f -ctime -$ctimeArg -print > $tmpDir/modifiedFiles.txt
    cat $tmpDir/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $tmpDir/sizes.new
    popd
    pushd $1
    cat $tmpDir/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $tmpDir/sizes.old 
    popd
    sdiff $tmpDir/sizes.old $tmpDir/sizes.new
    #rm -r $tmpDir
endif
