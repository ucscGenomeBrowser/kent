
CTC = ${HOME}/bin/${MACHTYPE}/checkTableCoords
GENEPREDS = genePredCTC
BEDS = bed3CTC bed4CTC bed8CTC bed12CTC
PSLS = pslCTC
TESTS = ${GENEPREDS} ${BEDS} ${PSLS}

test: run

clean:
	rm -fr output
	rm -f *.tab
	-hgsql test -e 'drop table chromInfo;'

load: clean
	hgsql test < input/fakeChromInfo.sql
	for TEST in ${GENEPREDS} ; do \
	  ldHgGene -predTab test $${TEST}Good input/$${TEST}Good.tab ; \
	  ldHgGene -predTab test $${TEST}Bad  input/$${TEST}Bad.tab ; \
	done
	for TEST in ${BEDS} ; do \
	  hgLoadBed test $${TEST}Good input/$${TEST}Good.bed ; \
	  hgLoadBed test $${TEST}Bad  input/$${TEST}Bad.bed ; \
	done
	for TEST in ${PSLS} ; do \
	 hgLoadPsl test -table=$${TEST}Good input/$${TEST}Good.psl ; \
	 hgLoadPsl test -table=$${TEST}Bad  input/$${TEST}Bad.psl ; \
	done

mkdirs:
	mkdir -p output

run: load mkdirs ${TESTS}

${TESTS}:
	${CTC} test ${@}Good
	-${CTC} test ${@}Bad > output/${@}Bad.out
	diff expected/${@}Bad.out output/${@}Bad.out

