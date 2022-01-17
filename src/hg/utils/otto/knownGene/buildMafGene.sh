#!/bin/sh -ex
cd $dir
{
. ./buildEnv.sh

if test "$multizDir" == ""
then
echo "Must set multizDir to directory with multiz files in it"
fi

if test "$mz" == ""
then
echo "Must set mz to name of multiz track"
fi

mkdir -p $multizDir/mafGene.knownGene${GENCODE_VERSION}
cd $multizDir/mafGene.knownGene${GENCODE_VERSION}

cat $multizDir/species.list | tr '[ ]' '[\n]' > order.list

export gp=knownGene
export I=0  
rm -rf exonAA exonNuc
mkdir exonAA exonNuc
for C in `sort -nk2 ../../../chrom.sizes | cut -f1`
do          
    I=`echo $I | awk '{print $1+1}'`
    echo "mafGene -chrom=$C -exons -noTrans $db $mz $gp order.list stdout | gzip -c > exonNuc/$C.exonNuc.fa.gz &"
    echo "mafGene -chrom=$C -exons $db $mz $gp order.list stdout | gzip -c > exonAA/$C.exonAA.fa.gz &"
    if [ $I -gt 11 ]; then
        echo "date"
        echo "wait"
        I=0
    fi
done > $gp.jobs
echo "date" >> $gp.jobs
echo "wait" >> $gp.jobs

time sh -x ./$gp.jobs 

time cat exonAA/*.gz > $gp.$mz.exonAA.fa.gz
time cat exonNuc/*.gz > $gp.$mz.exonNuc.fa.gz

export pd=/usr/local/apache/htdocs-hgdownload/goldenPath/$db/$mz/alignments
mkdir -p $pd
rm -f $pd/$gp.exonAA.fa.gz $pd/$gp.exonNuc.fa.gz
ln -s `pwd`/$gp.$mz.exonAA.fa.gz $pd/$gp.exonAA.fa.gz
ln -s `pwd`/$gp.$mz.exonNuc.fa.gz $pd/$gp.exonNuc.fa.gz

export gp=knownCanonical
export I=0  
rm -rf exonAA exonNuc knownCanonical
mkdir exonAA exonNuc knownCanonical

time cut -f1 $multizDir/../../chrom.sizes | while read C
do
    echo $C 1>&2
    hgsql $db -N -e "select chrom, chromStart, chromEnd, transcript from knownCanonical where chrom='$C'" > knownCanonical/$C.known.bed
done

ls knownCanonical/*.known.bed | while read F
do
  if [ -s $F ]; then
     echo $F | sed -e 's#knownCanonical/##; s/.known.bed//'
  fi
done | while read C
do
    echo "date"
    echo "mafGene -geneBeds=knownCanonical/$C.known.bed -exons -noTrans $db $mz knownGene order.list stdout | \
        gzip -c > exonNuc/$C.exonNuc.fa.gz &"
    echo "mafGene -geneBeds=knownCanonical/$C.known.bed -exons $db $mz knownGene order.list stdout | \
        gzip -c > exonAA/$C.exonAA.fa.gz &"
    if [ $I -gt 11 ]; then
        echo "date"
        echo "wait"
        I=0
    fi
done > $gp.$mz.jobs
echo "date" >> $gp.$mz.jobs
echo "wait" >> $gp.$mz.jobs

time sh -x $gp.$mz.jobs 

cat exonAA/c*.gz > $gp.$mz.exonAA.fa.gz
cat exonNuc/c*.gz > $gp.$mz.exonNuc.fa.gz

rm -rf exonAA exonNuc knownCanonical

export pd=/usr/local/apache/htdocs-hgdownload/goldenPath/$db/$mz/alignments
mkdir -p $pd
rm -f $pd/$gp.exonAA.fa.gz $pd/$gp.exonNuc.fa.gz
ln -s `pwd`/$gp.$mz.exonAA.fa.gz $pd/$gp.exonAA.fa.gz
ln -s `pwd`/$gp.$mz.exonNuc.fa.gz $pd/$gp.exonNuc.fa.gz
#cd  $pd
#md5sum *.fa.gz > md5sum.txt

echo "BuildMafGene successfully finished"
} > doMafGene.log < /dev/null 2>&1
