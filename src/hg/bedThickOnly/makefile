kentSrc = ../..
include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedThickOnly.o

bedThickOnly: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedThickOnly $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedThickOnly${EXE}

clean:
	rm -f $O
