kentSrc = ../../../..
include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = reTraceFixMaf.o

reTraceFixMaf: $O ${MYLIBS}
	${CC} ${COPT} -ggdb -o ${BINDIR}/reTraceFixMaf $O ${MYLIBS} $L

clean:
	rm -f $O
