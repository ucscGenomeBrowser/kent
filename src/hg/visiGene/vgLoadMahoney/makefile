kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgLoadMahoney.o mahoney.o

all: vgLoadMahoney

mahoney.h mahoney.c: mahoney.as
	autoSql mahoney.as mahoney

mahoney.o: mahoney.h mahoney.c

vgLoadMahoney.o: mahoney.o

vgLoadMahoney: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgLoadMahoney $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/vgLoadMahoney${EXE}

clean:
	rm -f $O
