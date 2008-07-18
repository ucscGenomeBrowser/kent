include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = positionalTblCheck.o

positionalTblCheck: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/positionalTblCheck $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/positionalTblCheck${EXE}

clean:
	rm -f $O
