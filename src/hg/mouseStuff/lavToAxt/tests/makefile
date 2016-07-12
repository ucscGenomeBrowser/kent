kentSrc = ../../../..
include ../../../../inc/common.mk

lavToAxt=${DESTBINDIR}/lavToAxt

all::

test: oldStyleBlastz newStyleLastz

newStyleLastz:	mkdirs
	@${lavToAxt} -tfa -fa -scoreScheme=input/$@.Q.txt input/$@.lav \
		output/hg19.chrM.fa output/susScr3.chrM.fa output/$@.axt
	@diff -u expected/$@.axt output/$@.axt
	@${lavToAxt} -scoreScheme=input/$@.Q.txt input/$@.lav \
		input/hg19.chrM.2bit input/susScr3.chrM.2bit output/$@.axt
	@diff -u expected/$@.axt output/$@.axt
	@${lavToAxt} -scoreScheme=input/$@.Q.txt input/$@.lav \
		output/nibTarget output/nibQuery output/$@.axt
	@diff -u expected/$@.axt output/$@.axt

oldStyleBlastz:	mkdirs
	@${lavToAxt} -tfa -fa -scoreScheme=input/$@.Q.txt input/$@.lav \
		output/hg19.chrM.fa output/susScr3.chrM.fa output/$@.axt
	@diff -u expected/$@.axt output/$@.axt
	@${lavToAxt} -scoreScheme=input/$@.Q.txt input/$@.lav \
		input/hg19.chrM.2bit input/susScr3.chrM.2bit output/$@.axt
	@diff -u expected/$@.axt output/$@.axt
	@${lavToAxt} -scoreScheme=input/$@.Q.txt input/$@.lav \
		output/nibTarget output/nibQuery output/$@.axt
	@diff -u expected/$@.axt output/$@.axt
	@${lavToAxt} input/crea.lav output output output/crea.axt
	@diff expected/crea.axt output/crea.axt
	@${lavToAxt} -tfa -fa input/crea.lav output/hCreaGeno.fa \
		output/mCreaGeno.fa output/crea.axt
	@diff expected/crea.axt output/crea.axt
	@${lavToAxt} input/crea.lav output/crea.2bit \
		output/crea.2bit output/crea.axt
	@diff expected/crea.axt output/crea.axt

mkdirs:
	@${MKDIR} output output/nibTarget output/nibQuery
	@twoBitToFa input/hg19.chrM.2bit output/hg19.chrM.fa
	@twoBitToFa input/susScr3.chrM.2bit output/susScr3.chrM.fa
	@twoBitToFa input/mCreaGeno.2bit output/mCreaGeno.fa
	@twoBitToFa input/hCreaGeno.2bit output/hCreaGeno.fa
	@faToTwoBit output/hCreaGeno.fa output/mCreaGeno.fa output/crea.2bit
	@faToNib -softMask output/mCreaGeno.fa output/mCreaGeno.nib > /dev/null
	@faToNib -softMask output/hCreaGeno.fa output/hCreaGeno.nib > /dev/null
	@faToNib -softMask output/hg19.chrM.fa output/nibTarget/hg19_chrM.nib > /dev/null
	@faToNib -softMask output/susScr3.chrM.fa output/nibQuery/susScr3_chrM.nib > /dev/null

clean::
	@rm -fr output
