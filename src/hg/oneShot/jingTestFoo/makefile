kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = jingTestFoo.o

jingTestFoo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/jingTestFoo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/jingTestFoo${EXE}

clean:
	rm -f $O
