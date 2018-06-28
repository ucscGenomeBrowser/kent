kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gsidMsa.o

gsidMsa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gsidMsa $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/gsidMsa${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	gsidMsa input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

clean::
	rm -f ${O}
