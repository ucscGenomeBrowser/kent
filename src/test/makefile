../cgi-bin/test.cgi: ../lib/$(MACHTYPE)/jkweb.a test.c
	${CC} ${COPT} ${CFLAGS} -I../inc test.c ../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x ${AOUT}
	mv ${AOUT} ../cgi-bin/test.cgi

testGenomeRangeTree: ../lib/$(MACHTYPE)/jkweb.a testGenomeRangeTree.c
	${CC} ${COPT} ${CFLAGS} -I../inc testGenomeRangeTree.c ../lib/$(MACHTYPE)/jkweb.a -lm -o testGenomeRangeTree
	chmod a+x testGenomeRangeTree
	./testGenomeRangeTree -verbose=0

test:
	./hgTest.pl -v -db hg13 | tee $(HOST).hg13.log  2>&1
	./hgTest.pl -v -db hg12 | tee $(HOST).hg12.log  2>&1
	./hgTest.pl -v -db hg11 | tee $(HOST).hg11.log  2>&1
	./hgTest.pl -v -db hg10 | tee $(HOST).hg10.log  2>&1
	./hgTest.pl -v -db mm2  | tee $(HOST).mm2.log   2>&1
	./hgTest.pl -v -db mm1  | tee $(HOST).mm1.log   2>&1
	./hgTest.pl -v -db rn1  | tee $(HOST).rn1.log   2>&1

