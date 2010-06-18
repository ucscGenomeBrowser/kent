include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = libScan.o

libScan: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/libScan $O $(MYLIBS) $L

clean::
	rm -f ${O}
