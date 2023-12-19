
HGLOADOUT=hgLoadOut

ifneq ($(wildcard ../hgLoadOut),)
  HGLOADOUT=../hgLoadOut
endif

test:	test0 test1 test2 test3

test0:
	${HGLOADOUT} -tabFile=chrCp.rmsk.tab -table=rmsk test chrCp.fa.out.gz
	@diff chrCp.expected.tab chrCp.rmsk.tab
	@rm -f chrCp.rmsk.tab

test1:
	zcat chrCp.fa.out.gz | ${HGLOADOUT} -tabFile=chrCp.rmsk.tab -table=rmsk test stdin
	@diff chrCp.expected.tab chrCp.rmsk.tab
	@rm -f chrCp.rmsk.tab

test2:
	${HGLOADOUT} -tabFile=triCas2.rmsk.tab -table=rmsk test https://hgdownload.soe.ucsc.edu/goldenPath/triCas2/bigZips/triCas2.fa.out.gz
	@md5sum triCas2.rmsk.tab > triCas2.md5sum
	@diff triCas2.md5sum triCas2.expected.md5sum
	@rm -f triCas2.rmsk.tab triCas2.md5sum

test3:
	${HGLOADOUT} -tabFile=stdout -table=rmsk test https://hgdownload.soe.ucsc.edu/hubs/GCA/001/708/105/GCA_001708105.1/GCA_001708105.1.repeatMasker.out.gz | gzip -c > GCA_001708105.1.rmsk.tab.gz
	@zdiff GCA_001708105.1.rmsk.tab.gz GCA_001708105.1.expected.tab.gz
	@rm -f GCA_001708105.1.rmsk.tab.gz

clean::
	rm -f chrCp.rmsk.tab
	rm -f triCas2.rmsk.tab triCas2.md5sum
	rm -f GCA_001708105.1.rmsk.tab.gz
