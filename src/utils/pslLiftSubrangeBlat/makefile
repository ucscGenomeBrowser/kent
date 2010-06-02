include ../../inc/common.mk
HG_INC += -I../../hg/inc

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pslLiftSubrangeBlat
O = pslLiftSubrangeBlat.o

pslLiftSubrangeBlat: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

test::
	cd tests && ${MAKE} test

clean::
	rm -f ${A}${EXE} ${O}
