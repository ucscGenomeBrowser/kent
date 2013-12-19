include ../../../../inc/common.mk

lavToAxt=${DESTBINDIR}/lavToAxt

all::

test: oldStyleBlastz newStyleLastz

newStyleLastz:	mkdirs
	${lavToAxt} -tfa -fa -scoreScheme=input/$@.Q.txt input/$@.lav \
		input/hg19.chrM.fa input/susScr3.chrM.fa output/$@.axt
	diff -u expected/$@.axt output/$@.axt

oldStyleBlastz:	mkdirs
	${lavToAxt} -tfa -fa -scoreScheme=input/$@.Q.txt input/$@.lav \
		input/hg19.chrM.fa input/susScr3.chrM.fa output/$@.axt
	diff -u expected/$@.axt output/$@.axt

mkdirs:
	@${MKDIR} output

clean::
	rm -fr output
