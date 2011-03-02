include ../inc/common.mk

../cgi-bin/test.cgi: ../lib/$(MACHTYPE)/jkweb.a test.c
	${CC} ${COPT} ${CFLAGS} -I../inc test.c ../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	chmod a+x ${AOUT}
	mv ${AOUT} ../cgi-bin/test.cgi

MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

testRangeTree: ${MYLIBS} testRangeTree.c
	${CC} ${COPT} ${CFLAGS} -I../inc -I../hg/inc testRangeTree.c ${MYLIBS} ${L} -lm -o testRangeTree
	chmod a+x testRangeTree
	./testRangeTree -verbose=0
	hexdump -f out/testRangeTree.fmt out/testRangeTree.out > out/testRangeTree.hexdump2
	diff out/testRangeTree.hexdump out/testRangeTree.hexdump2
	rm out/testRangeTree.out out/testRangeTree.hexdump2 testRangeTree

testGenomeRangeTree: ${MYLIBS} testGenomeRangeTree.c
	${CC} ${COPT} ${CFLAGS} -I../inc -I../hg/inc testGenomeRangeTree.c ${MYLIBS} ${L} -lm -o testGenomeRangeTree
	chmod a+x testGenomeRangeTree
	./testGenomeRangeTree -verbose=0
	hexdump -n 58 -f out/testGenomeRangeTree.fmt out/testGenomeRangeTree.out > out/testGenomeRangeTree.hexdump2
	diff --brief out/testGenomeRangeTree.hexdump out/testGenomeRangeTree.hexdump2
	diff --brief out/testGenomeRangeTree.bama out/testGenomeRangeTree.out
	diff --brief out/testGenomeRangeTree.bama out/testGenomeRangeTree.out2
	diff --brief out/testGenomeRangeTreeUnion1.bama     out/testGenomeRangeTreeUnion1.out
	diff --brief out/testGenomeRangeTreeUnion2.bama     out/testGenomeRangeTreeUnion2.out
	diff --brief out/testGenomeRangeTreeUnion_1or2.bama out/testGenomeRangeTreeUnion_1or2.out
	rm out/*.out out/*.out2 out/testGenomeRangeTree.hexdump2 testGenomeRangeTree

test:
	./hgTest.pl -v -db hg13 | tee $(HOST).hg13.log  2>&1
	./hgTest.pl -v -db hg12 | tee $(HOST).hg12.log  2>&1
	./hgTest.pl -v -db hg11 | tee $(HOST).hg11.log  2>&1
	./hgTest.pl -v -db hg10 | tee $(HOST).hg10.log  2>&1
	./hgTest.pl -v -db mm2  | tee $(HOST).mm2.log   2>&1
	./hgTest.pl -v -db mm1  | tee $(HOST).mm1.log   2>&1
	./hgTest.pl -v -db rn1  | tee $(HOST).rn1.log   2>&1

#mkout:
#	@${MKDIR} out

clean:

