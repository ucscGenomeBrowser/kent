include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bedSort.o

bedSort: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedSort $O $(MYLIBS) $L


lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}
