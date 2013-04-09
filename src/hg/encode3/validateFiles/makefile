kentSrc = ../../..
A = validateFiles
include $(kentSrc)/inc/userApp.mk

test::
	rm -rf output
	mkdir output
	-validateFiles -type=tagAlign -chromDb=hg18 -genome=/cluster/data/hg18/hg18.2bit input/tagAlign1.txt > output/tagAlign1.out 2>&1 
	cmp output/tagAlign1.out expected/tagAlign1.out
	-validateFiles -nMatch -chromDb=hg18 -type=tagAlign -genome=/cluster/data/hg18/hg18.2bit input/tagAlign2.txt > output/tagAlign2.out 2>&1 
	cmp output/tagAlign2.out expected/tagAlign2.out
	validateFiles  -type=BAM -chromDb=hg18  input/bam1.bam > output/bam1.out 2>&1 
	cmp output/bam1.out expected/bam1.out
	validateFiles  -type=bigWig -chromDb=hg18  input/bigWig1.bw > output/bigWig1.out 2>&1 
	cmp output/bigWig1.out expected/bigWig1.out
	-validateFiles  -type=bigWig -chromDb=hg18  input/bigWigTruncated.bw > output/bigWigTruncated.out 2>&1 
	cmp output/bigWigTruncated.out expected/bigWigTruncated.out
	validateFiles  -type=bed12 -chromDb=hg19 input/bed12ok.txt > output/bed12ok.out 2>&1 
	cmp output/bed12ok.out expected/bed12ok.out
	#-validateFiles  -type=bed12 -chromDb=hg19 input/bed12bad.txt > output/bed12bad.out  2>&1 
	#cmp output/bed12bad.out expected/bed12bad.out

# note "-" before the command means you know it will have an error, but continue making

clean::
	rm -fr output
