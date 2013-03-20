include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm -lz
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

