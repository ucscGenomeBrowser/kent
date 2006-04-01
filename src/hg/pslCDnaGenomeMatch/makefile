include ../../inc/common.mk
HG_WARN=-Wall

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = pslCDnaGenomeMatch.o 

pslCDnaGenomeMatch: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch $O $(MARKDEBUG) $(MYLIBS) $L 
#${STRIP} $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch${EXE}

lib:
	cd ../../lib && ${MAKE}

test: test1 test2
%:	input/%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=$@.bed input/$@.psl S1.len /cluster/data/hg17/bed/pseudo/blueMrna/mrnaNover.2bit /cluster/data/hg17/nib output/$@.psl -snp=input/$@.snp -score=output/$@.tab  > $@.log 
	diff expected/$@.psl output/$@.psl 
	diff expected/$@.tab output/$@.tab 
