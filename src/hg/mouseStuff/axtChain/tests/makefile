kentSrc = ../../../..
include ../../../../inc/common.mk

axtChain=${DESTBINDIR}/axtChain

all::

test: oldStyleBlastz newStyleLastz

newStyleLastz:	mkdirs
	@zcat input/$@.axt.gz > output/$@.axt
	@${axtChain} output/$@.axt \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.2bit -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.2bit output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} input/$@.axt.gz \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.2bit -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.2bit output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -faQ -faT input/$@.axt.gz \
		-minScore=3000 -linearGap=loose \
		output/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		output/susScr3.chrM.fa output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -psl -faQ -faT input/$@.psl \
		-minScore=3000 -linearGap=loose \
		output/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		output/susScr3.chrM.fa output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -psl input/$@.psl \
		-minScore=3000 -linearGap=loose \
		output/nibTarget -scoreScheme=input/$@.Q.txt \
		output/nibQuery output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain

oldStyleBlastz:	mkdirs
	@zcat input/$@.axt.gz > output/$@.axt
	@${axtChain} output/$@.axt \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.2bit -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.2bit output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} output/$@.axt \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.2bit -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.2bit output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -faQ -faT input/$@.axt.gz \
		-minScore=3000 -linearGap=loose \
		output/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		output/susScr3.chrM.fa output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -psl -faQ -faT input/$@.psl \
		-minScore=3000 -linearGap=loose \
		output/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		output/susScr3.chrM.fa output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain
	@${axtChain} -psl input/$@.psl \
		-minScore=3000 -linearGap=loose \
		output/nibTarget -scoreScheme=input/$@.Q.txt \
		output/nibQuery output/$@.chain > /dev/null 2>&1
	@diff -u expected/$@.chain output/$@.chain

mkdirs:
	@${MKDIR} output output/nibTarget output/nibQuery
	@twoBitToFa input/hg19.chrM.2bit output/hg19.chrM.fa
	@twoBitToFa input/susScr3.chrM.2bit output/susScr3.chrM.fa
	@faToNib -softMask output/hg19.chrM.fa output/nibTarget/hg19_chrM.nib > /dev/null
	@faToNib -softMask output/susScr3.chrM.fa output/nibQuery/susScr3_chrM.nib > /dev/null

clean::
	@rm -fr output
