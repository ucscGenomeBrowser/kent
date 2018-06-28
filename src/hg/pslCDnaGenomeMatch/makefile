kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

M = pslCDnaGenomeMatch.o  
DB=hg18

O = pslMismatch.o misMatch.o 
pslCDnaGenomeMatch: $M $(MYLIBS)
	gcc -ggdb -o ${BINDIR}/pslCDnaGenomeMatch $M $(MARKDEBUG) $(MYLIBS) $L 
#${STRIP} ${BINDIR}/pslCDnaGenomeMatch${EXE}

misMatch: $O $(MYLIBS)
	gcc -ggdb -o ${BINDIR}/misMatch  $O $(MARKDEBUG) $(MYLIBS) $L 

lib:
	cd ../../lib && ${MAKE}

test: test14 test15 test16 mrna1 mrna5 mrna13b mrna13c rest1 testDA9 pass1733 mrnaDrop 
t%:	input/t%.psl
	${MKDIR} output
	DB=hg18
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl /hive/users/baertsch/retro/${DB}/S1.len /hive/users/baertsch/retro/${DB}/est.2bit /hive/data/genomes/${DB}/nib output/$@.psl -score=output/$@.tab -minDiff=5  > $@.log 
	diff expected/$@.psl output/$@.psl 
	rm -f $@.log
r%:	input/r%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl /hive/users/baertsch/retro/mm9/S1.len /hive/users/baertsch/retro/mm9/est.2bit /hive/data/genomes/mm9/nib output/$@.psl -score=output/$@.tab -minDiff=3 -notAlignPenalty=3 > $@.log 
	diff expected/$@.psl output/$@.psl 
#rm -f $@.log
m%:	input/m%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl /hive/users/baertsch/retro/${DB}/S1.len /hive/data/genomes/${DB}/bed/mrnaBlastz/mrnaNoversion.2bit /hive/data/genomes/${DB}/nib output/$@.psl -score=output/$@.tab -notAlignPenalty=8 -minDiff=3  > $@.log 
	diff expected/$@.psl output/$@.psl 
#	diff expected/$@.tab output/$@.tab 
	rm -f $@.log
p%:	input/p%.psl
	${MKDIR} output
	pslCDnaGenomeMatch -verbose=4 -bedOut=output/$@.bed input/$@.psl /hive/users/baertsch/retro/${DB}/S1.len /hive/data/genomes/${DB}/bed/mrnaBlastz/mrnaNoversion.2bit /hive/data/genomes/${DB}/nib output/$@.psl -score=output/$@.tab -minDiff=5  -passthru > $@.log 
	diff expected/$@.psl output/$@.psl 
	rm -f $@.log
clean:
	rm -rf output
