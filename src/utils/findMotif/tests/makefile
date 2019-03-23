
testList = test1 test2 test3 test4 test5 test6 test7 test8 test9 \
	test10 test11

test: ${testList}

test1:
	@findMotif -verbose=3 -motif=TACTAAACCC hg19.chrM.fa.gz 2>&1 | grep -v "kent source version" > test1.txt 2>&1
	@zdiff -q test1.txt expected/test1.txt.gz
	@ rm -f test1.txt

test2:
	@findMotif -verbose=2 -motif=GGGTTTAGTA hg19.chrM.fa.gz 2>&1 | grep -v "kent source version" > test2.txt 2>&1
	@zdiff -q test2.txt expected/test2.txt.gz
	@ rm -f test2.txt

test3:
	@findMotif -misMatch=2 -verbose=2 -motif=TACTAAACCC hg19.chrM.fa.gz 2>&1 | grep -v "kent source version"  > test3.txt 2>&1
	@zdiff -q test3.txt expected/test3.txt.gz
	@ rm -f test3.txt

test4:
	@findMotif -misMatch=2 -verbose=2 -motif=GGGTTTAGTA hg19.chrM.fa.gz 2>&1 | grep -v "kent source version" > test4.txt 2>&1
	@zdiff -q test4.txt expected/test4.txt.gz
	@ rm -f test4.txt

test5:
	@findMotif -misMatch=2 -motif=TACTAAACCC hg19.chrM.fa.gz > test5.txt 2>&1
	@zdiff -q test5.txt expected/test5.txt.gz
	@ rm -f test5.txt

test6:
	@findMotif -misMatch=2 -motif=GGGTTTAGTA hg19.chrM.fa.gz > test6.txt 2>&1
	@zdiff -q test6.txt expected/test6.txt.gz
	@ rm -f test6.txt

test7:
	@findMotif -misMatch=3 -verbose=2 -motif=GGGTTTAGTA hg19.chrM.fa.gz 2>&1 | grep -v "kent source version" > test7.txt 2>&1
	@zdiff -q test7.txt expected/test7.txt.gz
	@ rm -f test7.txt

test8:
	@findMotif -misMatch=3 -motif=GGGTTTAGTA hg19.chrM.fa.gz > test8.txt 2>&1
	@zdiff -q test8.txt expected/test8.txt.gz
	@ rm -f test8.txt

# with error conditions

test9:
	@findMotif -misMatch=3 -motif=GGG hg19.chrM.fa.gz > test9.txt 2>&1 || true
	@zdiff -q test9.txt expected/test9.txt.gz
	@rm -f test9.txt

test10:
	@findMotif -misMatch=3 -motif=TTACATAGTACATAAAGTCATTTACCGTACATA hg19.chrM.fa.gz > test10.txt 2>&1 || true
	@zdiff -q test10.txt expected/test10.txt.gz
	@rm -f test10.txt

test11:
	@findMotif -misMatch=3 -motif=TACATAGTACATAAAGTCATTTACCGTACATA hg19.chrM.fa.gz > test11.txt 2>&1
	@zdiff -q test11.txt expected/test11.txt.gz
	@rm -f test11.txt


clean:
	rm -f test*.txt
