include ../../../../inc/common.mk

axtChain=${DESTBINDIR}/axtChain

all::

test: oldStyleBlastz newStyleLastz

newStyleLastz:	mkdirs
	${axtChain} -verbose=2 -faQ -faT input/$@.axt \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.fa output/$@.chain
	diff -u expected/$@.chain output/$@.chain

oldStyleBlastz:	mkdirs
	${axtChain} -verbose=2 -faQ -faT input/$@.axt \
		-minScore=3000 -linearGap=loose \
		input/hg19.chrM.fa -scoreScheme=input/$@.Q.txt \
		input/susScr3.chrM.fa output/$@.chain
	diff -u expected/$@.chain output/$@.chain

mkdirs:
	@${MKDIR} output

clean::
	rm -fr output
