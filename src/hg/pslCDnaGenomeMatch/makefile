include ../../inc/common.mk
HG_WARN=-Wall

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 


M = pslCDnaGenomeMatch.o  

O = pslMismatch.o misMatch.o 
pslCDnaGenomeMatch: $M $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch $M $(MARKDEBUG) $(MYLIBS) $L 
#${STRIP} $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch${EXE}

misMatch: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/misMatch  $O $(MARKDEBUG) $(MYLIBS) $L 

lib:
	cd ../../lib && ${MAKE}

testNew: test14 test15 test16 test1 test5
%:	input/%.psl
	${MKDIR} output2
	misMatch -verbose=4 -bedOut=output2/$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/est.2bit /cluster/data/hg18/nib output2/$@.psl -score=output2/$@.tab  > $@.2.log 
	diff expected/$@.psl output2/$@.psl 
#diff expected/$@.tab output2/$@.tab 

test: testDA9 test14 test15 test16 mrna1 mrna5 testBQ mrna13b mrna13c mrna12a pass1733 mrnaDrop
t%:	input/t%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/est.2bit /cluster/data/hg18/nib output/$@.psl -score=output/$@.tab -minDiff=5  > $@.log 
	diff expected/$@.psl output/$@.psl 
m%:	input/m%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/mrnaNoversion.2bit /cluster/data/hg18/nib output/$@.psl -score=output/$@.tab -minDiff=5  > $@.log 
	diff expected/$@.psl output/$@.psl 
#	diff expected/$@.tab output/$@.tab 
p%:	input/p%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/mrnaNoversion.2bit /cluster/data/hg18/nib output/$@.psl -score=output/$@.tab -minDiff=5  -passthru > $@.log 
	diff expected/$@.psl output/$@.psl 
