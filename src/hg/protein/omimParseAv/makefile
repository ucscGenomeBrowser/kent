kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = omimParseAv.o

omimParseAv: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/omimParseAv $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/omimParseAv${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	omimParseAv input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

clean::
	rm -f ${O}
