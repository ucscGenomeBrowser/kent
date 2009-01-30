#!/bin/tcsh -efx
#
# This program runs a test on the bigWig system that involves about 5 gig of i/o and
# a couple of minutes of CPU time.
#

mkdir -p bigTest
cd bigTest
wigTestMaker wig
mkdir -p big sum
cd wig
wigToBigWig increasing.wig chrom.sizes ../big/increasing.bigWig
wigToBigWig mixem.wig chrom.sizes ../big/mixem.bigWig
wigToBigWig shortBed.wig chrom.sizes ../big/shortBed.bigWig
wigToBigWig shortVar.wig chrom.sizes ../big/shortVar.bigWig
wigToBigWig shortFixed.wig chrom.sizes ../big/shortFixed.bigWig
wigToBigWig sineSineBed.wig chrom.sizes ../big/sineSineBed.bigWig
wigToBigWig sineSineVar.wig chrom.sizes ../big/sineSineVar.bigWig
wigToBigWig sineSineFixed.wig chrom.sizes ../big/sineSineFixed.bigWig
cd ../big
bigWigSummary increasing.bigWig chr1 0 10000000 1 > ../sum/increasing.1
bigWigSummary increasing.bigWig chr1 0 10000000 10 > ../sum/increasing.11
bigWigSummary increasing.bigWig chr1 0 10000000 100 > ../sum/increasing.111
bigWigToBedGraph increasing.bigWig -chrom=chr1 -start=222222 -end=222242 ../sum/increasing.bed

bigWigToBedGraph mixem.bigWig ../sum/mixem.bed
bigWigToBedGraph shortBed.bigWig ../sum/shortBed.bed
bigWigToBedGraph shortVar.bigWig ../sum/shortVar.bed
bigWigToBedGraph shortFixed.bigWig ../sum/shortFixed.bed

bigWigSummary mixem.bigWig chr1 0 1000 1 > ../sum/mixem.1
bigWigSummary shortBed.bigWig chr1 0 1000 1 > ../sum/shortBed.1
bigWigSummary shortVar.bigWig chr1 0 1000 1 > ../sum/shortVar.1
bigWigSummary shortFixed.bigWig chr1 0 1000 1 > ../sum/shortFixed.1

bigWigSummary sineSineFixed.bigWig chr3 0 100000000 10 > ../sum/ssf.100M
bigWigSummary sineSineFixed.bigWig chr3 0 10000000 10 > ../sum/ssf.10M
bigWigSummary sineSineFixed.bigWig chr3 0 1000000 10 > ../sum/ssf.1M
bigWigSummary sineSineFixed.bigWig chr3 0 100000 10 > ../sum/ssf.100K
bigWigSummary sineSineFixed.bigWig chr3 0 10000 10 > ../sum/ssf.10K
bigWigSummary sineSineFixed.bigWig chr3 0 1000 10 > ../sum/ssf.1K
bigWigSummary sineSineFixed.bigWig chr3 0 100 10 > ../sum/ssf.100
bigWigSummary sineSineFixed.bigWig chr3 0 10 10 > ../sum/ssf.1O
bigWigSummary sineSineFixed.bigWig chr3 0 1 10 > ../sum/ssf.1

bigWigSummary sineSineVar.bigWig chr2 0 10000000 10 > ../sum/ssv.10M
bigWigSummary sineSineVar.bigWig chr2 0 100000 10 > ../sum/ssv.100K
bigWigSummary sineSineVar.bigWig chr2 0 1000 10 > ../sum/ssv.1K
bigWigSummary sineSineVar.bigWig chr2 0 10 10 > ../sum/ssv.1O
bigWigSummary sineSineVar.bigWig chr2 0 1 10 > ../sum/ssv.1

bigWigSummary sineSineBed.bigWig chr1 0 10000000 10 > ../sum/ssb.10M
bigWigSummary sineSineBed.bigWig chr1 0 100000 10 > ../sum/ssb.100K
bigWigSummary sineSineBed.bigWig chr1 0 1000 10 > ../sum/ssb.1K
bigWigSummary sineSineBed.bigWig chr1 0 10 10 > ../sum/ssb.1O
bigWigSummary sineSineBed.bigWig chr1 0 1 10 > ../sum/ssb.1
