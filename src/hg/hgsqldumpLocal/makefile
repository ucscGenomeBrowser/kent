include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsqldumpLocal.o

hgsqldumpLocal: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgsqldumpLocal $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgsqldumpLocal${EXE}

clean:
	rm -f $O
