kentSrc = ../..
include ../../inc/common.mk

JKSRCDIR = ../..
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBS =  $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a

O = makeSeqs.o

makeSeqs: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv ${AOUT} makeSeqs
	${STRIP} makeSeqs${EXE}
	chmod a+rx makeSeqs
	cp makeSeqs ${BINDIR}

clean::
	rm -f ${O}
