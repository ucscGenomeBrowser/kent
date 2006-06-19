#!/bin/csh -ef

# kgProtBlast queryFile
#
# Align Known Genes proteins to mRNAs and convert to PSL format
#
# Assumes:
#  - blast database kgMrna.fa
#  - tab seperate file kgProtMrna.pairs, of pairs of protein and mrna
#    accessions
#
# Output created in a hierarchy under psl.tmp in current directory
#    

set blastDir=/iscratch/blast
set tmpDir=/scratch/tmp

if ($#argv != 1) then
    echo "wrong # args: kgProtBlast queryFile" >/dev/stderr
    exit 1
endif

set queryFile=$1

set dbDir=.
set selectFile=$dbDir/kgProtMrna.pairs
set dataBase=$dbDir/kgMrna.fa

# check for input files upfront
foreach f ($queryFile $selectFile $dataBase)
    if (! -e $f) then
        echo "kgProtBlast input file does not exist: $f" >/dev/stderr
        exit 1
    endif
end

# use last to chars of name to make outdir
set outBase=`basename $queryFile .fa`
set outDir2=`echo $outBase |awk '{print substr($0,length($0)-1)}'`
set outDir=psl.tmp/$outDir2

set outPslBase=$outBase.psl.gz
set outPsl=$outDir/$outPslBase

# output to local disk until complete to keep fewer global files open.
set outTmp=$tmpDir/$outPslBase

# align and converty to PSL. don't let blast filter query (-F F)
$blastDir/blastall -p tblastn -F F -d $dataBase -i $queryFile \
    | blastToPsl stdin stdout \
    | pslSelect -qtPairs=$selectFile stdin stdout \
    | gzip -1c >$outTmp

# install in output dir/
mkdir -p $outDir
cp -f $outTmp $outPsl.tmp
rm -f $outTmp
mv -f $outPsl.tmp  $outPsl
