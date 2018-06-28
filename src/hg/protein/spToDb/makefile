kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spToDb.o

spToDb: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/spToDb $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/spToDb${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	${BINDIR}/spToDb input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

clean::
	rm -f ${O}
