include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsqlLocal.o

hgsqlLocal: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgsqlLocal $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgsqlLocal${EXE}

clean:
	rm -f $O
