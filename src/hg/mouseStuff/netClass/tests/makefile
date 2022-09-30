
NETCLASS=netClass

ifneq ($(wildcard ../netClass),)
  NETCLASS=../netClass
endif

test:	test0

test0:
	${NETCLASS} -verbose=0 -noAr dm6.apiMel4.noClass.net.gz dm6 apiMel4 stdout | gzip -c > dm6.apiMel4.net.gz
	@zdiff dm6.apiMel4.net.gz dm6.apiMel4.expected.net.gz
	@rm -f dm6.apiMel4.net.gz
