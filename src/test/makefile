../cgi-bin/test.cgi: ../lib/$(MACHTYPE)/jkweb.a test.c
	gcc -I../inc test.c ../lib/$(MACHTYPE)/jkweb.a -lm
	chmod a+x a.out
	mv a.out ../cgi-bin/test.cgi

test:
	./hgTest.pl -v -db hg12 | tee $(HOST).hg12.log  2>&1
	./hgTest.pl -v -db hg11 | tee $(HOST).hg11.log  2>&1
	./hgTest.pl -v -db hg10 | tee $(HOST).hg10.log  2>&1
	./hgTest.pl -v -db hg8  | tee $(HOST).hg8.log   2>&1
	./hgTest.pl -v -db mm2  | tee $(HOST).mm2.log   2>&1
	./hgTest.pl -v -db mm1  | tee $(HOST).mm1.log   2>&1

