kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gsidAaMsa.o

gsidAaMsa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gsidAaMsa $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/gsidAaMsa${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	gsidAaMsa input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output


clean::
	rm -f ${O}
