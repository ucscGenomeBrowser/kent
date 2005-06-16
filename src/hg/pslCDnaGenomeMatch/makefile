include ../../inc/common.mk
HG_WARN=-Wall

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
OPTIONS = -minAli=0.98 -nearTop=0.005 -verbose=4
MARKDEBUG =
DB = hg17
MRNA = mrnaHg17.fa 
DBDIR = /cluster/data/$(DB)
BED = $(DBDIR)/bed
PSEUDO = $(DBDIR)/bed/pseudo
BUILD=gs.17/build34

O = pslCDnaGenomeMatch.o 

pslCDnaGenomeMatch: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch $O $(MARKDEBUG) $(MYLIBS) $L 
#strip $(HOME)/bin/$(MACHTYPE)/pslCDnaGenomeMatch

lib:
	cd ../../lib && ${MAKE}

#test: chr3 chr9 chr11 chr13_random
test: test1 test2
%:	input/%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=5 -bedOut=$@.bed input/$@.psl S1.len input/S1.lst /cluster/data/hg17/bed/pseudo/blueMrna/mrnaNover.2bit /cluster/data/hg17/nib output/$@.psl -snp=input/$@.snp -score=output/$@.tab  > $@.log 
	#pslCDnaGenomeMatch -species=input/chimp$@.psl -verbose=5 -bedOut=$@.bed  input/$@.psl S1.len input/S1.lst /cluster/data/hg17/bed/pseudo/mrnaHg17.2bit /cluster/data/hg17/nib input/$@.snp output/$@.tab  > $@.log 
	diff expected/$@.psl output/$@.psl 
	diff expected/$@.tab output/$@.tab 
