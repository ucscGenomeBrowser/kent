kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lz
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = freen.o

hello: freen.o
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/freen $O $(MYLIBS) $L

testGlob:
	cd fooDir
	cd fooDir; echo f*.*

clean::
	rm -f ${O}

