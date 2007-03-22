include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = makeOrthoAtgTable.o

makeOrthoAtgTable: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/makeOrthoAtgTable $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/makeOrthoAtgTable${EXE}

clean:
	rm -f $O
