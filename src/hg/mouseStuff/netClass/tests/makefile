
SHELL=/bin/bash -o pipefail

NETCLASS=netClass

ifneq ($(wildcard ../netClass),)
  NETCLASS=../netClass
endif

test:	test0

newStyle:	test1 test2

test0:
	${NETCLASS} -verbose=0 -noAr dm6.apiMel4.noClass.net.gz dm6 apiMel4 stdout | gzip -c > dm6.apiMel4.net.gz
	@zdiff dm6.apiMel4.net.gz dm6.apiMel4.expected.net.gz
	@rm -f dm6.apiMel4.net.gz

test1:
	${NETCLASS} -verbose=0 -noAr -qSizes=https://hgdownload-test.gi.ucsc.edu/goldenPath/apiMel4/bigZips/apiMel4.chrom.sizes  dm6.apiMel4.noClass.net.gz dm6 apiMel4 stdout | gzip -c > dm6.apiMel4.net.gz
	@zdiff dm6.apiMel4.net.gz dm6.apiMel4.expected.net.gz
	@rm -f dm6.apiMel4.net.gz

test2:
	${NETCLASS} -verbose=0 -noAr -qSizes=/hive/data/genomes/apiMel4/chrom.sizes  dm6.apiMel4.noClass.net.gz dm6 apiMel4 stdout | gzip -c > dm6.apiMel4.net.gz
	@zdiff dm6.apiMel4.net.gz dm6.apiMel4.expected.net.gz
	@rm -f dm6.apiMel4.net.gz

clean:
	@rm -f dm6.apiMel4.net.gz
