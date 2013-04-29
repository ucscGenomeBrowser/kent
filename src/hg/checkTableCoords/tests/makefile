include ../../../inc/common.mk

CTC = ${DESTBINDIR}/checkTableCoords
GENEPREDS = genePredCTC
hgsql=${DESTBINDIR}/hgsql
ldHgGene=${DESTBINDIR}/ldHgGene
hgLoadBed=${DESTBINDIR}/hgLoadBed
hgLoadPsl=${DESTBINDIR}/hgLoadPsl
BEDS = bed3CTC bed4CTC bed8CTC bed12CTC
PSLS = pslCTC
TESTS = ${GENEPREDS} ${BEDS} ${PSLS}

test: run

clean:
	rm -fr output
	rm -f *.tab
	${hgsql} test -e 'drop table chromInfo;' > /dev/null 2>&1; /bin/true

load: clean
	${hgsql} test < input/fakeChromInfo.sql
	for TEST in ${GENEPREDS} ; do \
	  ${ldHgGene} -nobin -predTab test $${TEST}Good input/$${TEST}Good.tab -verbose=0 ; \
	  ${ldHgGene} -nobin -predTab test $${TEST}Bad  input/$${TEST}Bad.tab -verbose=0 ; \
	done
	for TEST in ${BEDS} ; do \
	  ${hgLoadBed} test $${TEST}Good input/$${TEST}Good.bed -verbose=0 ; \
	  ${hgLoadBed} -noStrict test $${TEST}Bad  input/$${TEST}Bad.bed -verbose=0 ; \
	done
	for TEST in ${PSLS} ; do \
	 ${hgLoadPsl} -noHistory test -table=$${TEST}Good input/$${TEST}Good.psl -verbose=0 ; \
	 ${hgLoadPsl} -noHistory test -table=$${TEST}Bad  input/$${TEST}Bad.psl -verbose=0 ; \
	done

mkdirs:
	mkdir -p output

run: load mkdirs ${TESTS}

${TESTS}:
	${CTC} test ${@}Good
	-${CTC} test ${@}Bad -verbose=2 > output/${@}Bad.out
	diff expected/${@}Bad.out output/${@}Bad.out
