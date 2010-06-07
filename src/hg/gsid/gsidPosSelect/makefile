include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gsidPosSel.o

gsidPosSel: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gsidPosSel $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gsidPosSel${EXE}

clean:
	rm -f $O
