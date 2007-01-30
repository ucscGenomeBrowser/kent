include ../../inc/common.mk
HG_WARN=-Wall

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = pslMismatch.o misMatch.o 

M = pslCDnaGenomeMatch.o  

misMatch: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/misMatch  $O $(MARKDEBUG) $(MYLIBS) $L 

pslCDnaGenomeMatch: $M $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch $M $(MARKDEBUG) $(MYLIBS) $L 
#${STRIP} $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch${EXE}
lib:
	cd ../../lib && ${MAKE}

test: test14 test15 test16 test1 test2
%:	input/%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=5 -bedOut=$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/est.2bit /cluster/data/hg18/nib output/$@.psl -score=output/$@.tab  > $@.log 
	diff expected/$@.psl output/$@.psl 
	diff expected/$@.tab output/$@.tab 

testNew: test14 test15 test16 test1 test2
%:	input/%.psl
	${MKDIR} output2
	misMatch -verbose=5 -bedOut=$@.bed input/$@.psl S1.hg18.len /cluster/data/hg18/bed/pseudo/est.2bit /cluster/data/hg18/nib output2/$@.psl -score=output2/$@.tab  > $@.2.log 
	diff expected/$@.psl output2/$@.psl 
	diff expected/$@.tab output2/$@.tab 

