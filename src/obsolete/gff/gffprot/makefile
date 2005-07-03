gffprot: gffprot.c common.o dnautil.o
	cc gffprot.c common.o dnautil.o
	mv ${AOUT} gffprot

common.o: common.c
	cc -c common.c
dnautil.o: dnautil.c
	cc -c dnautil.c
