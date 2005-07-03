gffinfo: gffinfo.c common.o dnautil.o
	cc gffinfo.c common.o dnautil.o
	mv ${AOUT} gffinfo

common.o: common.c
	cc -c common.c
dnautil.o: dnautil.c
	cc -c dnautil.c
