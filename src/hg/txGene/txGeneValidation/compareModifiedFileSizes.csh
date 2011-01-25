#!/bin/tcsh -efx

#
# compareModifiedFileSizes.csh: look at the sizes of recently created / modified files,
#     and compare their sizes to those of (comparable) earlier versions of the same
#     files.
#
echo $#
if ( $#argv < 4 ) then
    echo "compareModifiedFileSizes.csh: compare the size of recently-modified files against previous versions" 
    echo "usage"
    echo " compareModifiedFileSizes.csh <tmpDir> <newSizeFilename> <oldDir> <oldSizeFilename> [ctimeArg]"
    echo "options:"
    echo "- tmpDir: a temporary directory to hold the results"
    echo "- newSizeFilename: filename to store the new sizes of the newly-modified files"
    echo "- oldDir: the directory with the old versions of the files.  The new versions are assumed to be in the current directory"
    echo "- oldSizeFilename: filename to store the old sizes of the newly-modified files"
    echo "- [ctimeArg]: argument for ctime to indicate files of what age to select (default: 1 day old)"
else
    set ctimeArg = -1
    if ( $#argv > 4 ) then
        set ctimeArg = $5
    endif
    find . -type f -ctime $ctimeArg -print > $1/modifiedFiles.txt
    cat $1/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $1/$2 
    pushd $3
    cat $1/modifiedFiles.txt | sed 's/^/wc -l /' |bash > $1/$4 
    popd
    sdiff $1/$2 $1/$4
endif
