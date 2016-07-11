kentSrc = ../../..
include ../../../inc/common.mk

L = -l ssl -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
OPTIONS = -minAli=0.98 -nearTop=0.005 -verbose=4
MARKDEBUG =
DB = hg17
MRNA = mrnaHg17.fa 
DBDIR = /cluster/data/$(DB)
BED = $(DBDIR)/bed
PSEUDO = $(DBDIR)/bed/pseudo
BUILD=gs.17/build34

O = checkExp.o 

checkExp: $O $(MYLIBS)
	gcc -ggdb -o ${BINDIR}/checkExp $O $(MARKDEBUG) $(MYLIBS) $L 
#${STRIP} ${BINDIR}/checkExp${EXE}

lib:
	cd ../../lib && ${MAKE}

test: chr3 chr9 chr11 chr13_random
%:	input/%.bed
	${MKDIR} output
	checkExp -verbose=4 input/$@.bed S1.len $(PSEUDO)/all_mrna.psl input/S1.lst /cluster/data/hg17/bed/pseudo/stats/checkExp/mrna.2bit /cluster/data/hg17/bed/blastzSelf/axtChain/chain/$@.chain.gz /cluster/data/hg17/nib output/$@.tab  > $@.log 
	diff expected/$@.tab output/$@.tab 

clean::
	rm -f ${O}
